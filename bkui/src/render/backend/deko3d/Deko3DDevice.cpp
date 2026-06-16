#include <bkui/render/backend/deko3d/Deko3DDevice.hpp>
#include <bkui/core/FileSystem.hpp>

#include <deko3d.hpp>
#include <switch.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace bk
{
namespace
{
static constexpr std::uint32_t kFramebufferCount = 2;
static constexpr std::uint32_t kStaticCmdSize = 0x20000;
static constexpr std::uint32_t kImagePoolSize = 32 * 1024 * 1024;
static constexpr std::uint32_t kCodePoolSize = 128 * 1024;
static constexpr std::uint32_t kDataPoolSize = 16 * 1024 * 1024;
static constexpr std::uint32_t kTextureDescriptorCount = 32;

struct DkVertex
{
    float position[2];
    float color[3];
};

struct DkShaderBinaryHeader
{
    std::uint32_t magic;
    std::uint32_t headerSize;
    std::uint32_t controlSize;
    std::uint32_t codeSize;
    std::uint32_t programsOffset;
    std::uint32_t numPrograms;
};

class LinearPool
{
public:
    LinearPool() = default;

    LinearPool(dk::Device device, std::uint32_t flags, std::size_t size)
        : device_(device)
    {
        block_ = dk::MemBlockMaker{device_, static_cast<std::uint32_t>(size)}.setFlags(flags).create();
        cpu_ = block_.getCpuAddr();
        gpu_ = block_.getGpuAddr();
        size_ = size;
        cursor_ = 0;
    }

    bool valid() const
    {
        return static_cast<bool>(block_);
    }

    struct Allocation
    {
        dk::MemBlock block;
        std::size_t offset = 0;
        std::size_t size = 0;
        void* cpu = nullptr;
        DkGpuAddr gpu = DK_GPU_ADDR_INVALID;

        explicit operator bool() const
        {
            return static_cast<bool>(block);
        }
    };

    Allocation allocate(std::size_t size, std::size_t alignment = DK_MEMBLOCK_ALIGNMENT)
    {
        if (!valid() || size == 0 || alignment == 0)
        {
            return {};
        }

        const std::size_t aligned = (cursor_ + alignment - 1) & ~(alignment - 1);
        if (aligned + size > size_)
        {
            return {};
        }

        Allocation alloc;
        alloc.block = block_;
        alloc.offset = aligned;
        alloc.size = size;
        alloc.cpu = static_cast<std::uint8_t*>(cpu_) + aligned;
        alloc.gpu = gpu_ + aligned;
        cursor_ = aligned + size;
        return alloc;
    }

private:
    dk::Device device_{};
    dk::MemBlock block_{};
    void* cpu_ = nullptr;
    DkGpuAddr gpu_ = DK_GPU_ADDR_INVALID;
    std::size_t size_ = 0;
    std::size_t cursor_ = 0;
};

struct ShaderBlob
{
    std::vector<std::uint8_t> control;
    LinearPool::Allocation codeMem;
    dk::Shader shader;
};

struct DkBufferResource
{
    LinearPool::Allocation allocation;
};

struct DkTextureResource
{
    dk::Image image;
    dk::ImageDescriptor imageDescriptor;
    dk::SamplerDescriptor samplerDescriptor;
    LinearPool::Allocation imageMemory;
    LinearPool::Allocation stagingMemory;
    std::uint32_t imageDescriptorIndex = 0;
    std::uint32_t samplerDescriptorIndex = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct DkShaderResource
{
    ShaderBlob blob;
    ShaderStage stage = ShaderStage::Vertex;
};

struct DkPipelineResource
{
    ShaderHandle vertexShader;
    ShaderHandle fragmentShader;
    VertexLayoutDesc vertexLayout;
    std::vector<VertexAttributeDesc> attributes;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

struct DkCommandResource
{
    PipelineHandle pipeline;
    BufferHandle vertexBuffer;
    TextureHandle texture;
    std::uint32_t vertexCount = 0;
    std::uint32_t firstVertex = 0;
};

template <typename T>
T* Lookup(std::vector<T>& items, std::uint32_t id)
{
    if (id == 0 || id > items.size())
    {
        return nullptr;
    }

    return &items[id - 1];
}

class Deko3DDevice final : public Device
{
public:
    explicit Deko3DDevice(Platform& platform)
    {
        (void)platform;
    }

    bool Init() override
    {
        device_ = dk::DeviceMaker{}.create();
        queue_ = dk::QueueMaker{device_}.setFlags(DkQueueFlags_Graphics).create();
        imagePool_.emplace(device_, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, kImagePoolSize);
        codePool_.emplace(device_, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, kCodePoolSize);
        dataPool_.emplace(device_, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, kDataPoolSize);

        cmdBuf_ = dk::CmdBufMaker{device_}.create();
        cmdMem_ = dataPool_->allocate(kStaticCmdSize);
        if (!cmdMem_)
        {
            return false;
        }
        cmdBuf_.addMemory(cmdMem_.block, cmdMem_.offset, cmdMem_.size);

        textureDescriptorMemory_ = dataPool_->allocate(
            kTextureDescriptorCount * sizeof(DkImageDescriptor),
            DK_IMAGE_DESCRIPTOR_ALIGNMENT
        );
        samplerDescriptorMemory_ = dataPool_->allocate(
            kTextureDescriptorCount * sizeof(DkSamplerDescriptor),
            DK_SAMPLER_DESCRIPTOR_ALIGNMENT
        );
        if (!textureDescriptorMemory_ || !samplerDescriptorMemory_)
        {
            return false;
        }

        return CreateFramebufferResources();
    }

    void Shutdown() override
    {
        queue_.waitIdle();

        commandBuffers_.clear();
        pipelines_.clear();
        shaders_.clear();
        textures_.clear();
        buffers_.clear();

        DestroyFramebufferResources();

        cmdBuf_.clear();

    }

    void Resize(Vector2 size) override
    {
        framebufferWidth_ = static_cast<float>(size.x);
        framebufferHeight_ = static_cast<float>(size.y);
    }

    SwapchainHandle GetMainSwapchain() const override
    {
        return SwapchainHandle{1};
    }

    BufferHandle CreateBuffer(const BufferDesc& desc) override
    {
        if (desc.size == 0 || desc.data == nullptr)
        {
            return {};
        }

        DkBufferResource buffer;
        buffer.allocation = dataPool_->allocate(desc.size, 16);
        if (!buffer.allocation)
        {
            return {};
        }

        std::memcpy(buffer.allocation.cpu, desc.data, desc.size);
        buffers_.push_back(buffer);
        return BufferHandle{static_cast<std::uint32_t>(buffers_.size())};
    }

    bool UpdateBuffer(BufferHandle handle, const BufferDesc& desc) override
    {
        DkBufferResource* buffer = Lookup(buffers_, handle.id);
        if (buffer == nullptr || desc.size == 0 || desc.data == nullptr || desc.size > buffer->allocation.size)
        {
            return false;
        }

        std::memcpy(buffer->allocation.cpu, desc.data, desc.size);
        return true;
    }

    TextureHandle CreateTexture(const TextureDesc& desc) override
    {
        if (desc.width == 0 || desc.height == 0 || desc.rgba == nullptr ||
            nextTextureDescriptor_ >= kTextureDescriptorCount)
        {
            return {};
        }

        DkTextureResource texture;
        texture.width = desc.width;
        texture.height = desc.height;
        texture.imageDescriptorIndex = nextTextureDescriptor_;
        texture.samplerDescriptorIndex = nextTextureDescriptor_;
        ++nextTextureDescriptor_;

        dk::ImageLayout layout;
        dk::ImageLayoutMaker{device_}
            .setFlags(0)
            .setFormat(DkImageFormat_RGBA8_Unorm)
            .setDimensions(desc.width, desc.height)
            .initialize(layout);

        texture.imageMemory = imagePool_->allocate(layout.getSize(), layout.getAlignment());
        texture.stagingMemory = dataPool_->allocate(static_cast<std::size_t>(desc.width) * desc.height * 4, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
        if (!texture.imageMemory || !texture.stagingMemory)
        {
            return {};
        }

        std::memcpy(texture.stagingMemory.cpu, desc.rgba, static_cast<std::size_t>(desc.width) * desc.height * 4);
        texture.image.initialize(layout, texture.imageMemory.block, texture.imageMemory.offset);

        dk::ImageView imageView{texture.image};
        texture.imageDescriptor.initialize(imageView);
        dk::Sampler sampler{};
        sampler.setFilter(DkFilter_Linear, DkFilter_Linear, DkMipFilter_None);
        sampler.setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge);
        texture.samplerDescriptor.initialize(sampler);

        cmdBuf_.pushData(
            textureDescriptorMemory_.gpu + texture.imageDescriptorIndex * sizeof(DkImageDescriptor),
            &texture.imageDescriptor,
            sizeof(DkImageDescriptor)
        );
        cmdBuf_.pushData(
            samplerDescriptorMemory_.gpu + texture.samplerDescriptorIndex * sizeof(DkSamplerDescriptor),
            &texture.samplerDescriptor,
            sizeof(DkSamplerDescriptor)
        );
        cmdBuf_.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
        dk::ImageView dstView{texture.image};
        cmdBuf_.copyBufferToImage(
            {texture.stagingMemory.gpu},
            dstView,
            {0, 0, 0, desc.width, desc.height, 1}
        );
        queue_.submitCommands(cmdBuf_.finishList());
        queue_.waitIdle();
        cmdBuf_.clear();
        cmdBuf_.addMemory(cmdMem_.block, cmdMem_.offset, cmdMem_.size);

        textures_.push_back(texture);
        return TextureHandle{static_cast<std::uint32_t>(textures_.size())};
    }

    bool UpdateTexture(TextureHandle handle, const TextureDesc& desc) override
    {
        DkTextureResource* texture = Lookup(textures_, handle.id);
        if (texture == nullptr || desc.width == 0 || desc.height == 0 || desc.rgba == nullptr ||
            texture->width != desc.width || texture->height != desc.height)
        {
            return false;
        }

        std::memcpy(texture->stagingMemory.cpu, desc.rgba, static_cast<std::size_t>(desc.width) * desc.height * 4);

        cmdBuf_.clear();
        cmdBuf_.addMemory(cmdMem_.block, cmdMem_.offset, cmdMem_.size);
        dk::ImageView dstView{texture->image};
        cmdBuf_.copyBufferToImage(
            {texture->stagingMemory.gpu},
            dstView,
            {0, 0, 0, desc.width, desc.height, 1}
        );
        queue_.submitCommands(cmdBuf_.finishList());
        queue_.waitIdle();
        cmdBuf_.clear();
        cmdBuf_.addMemory(cmdMem_.block, cmdMem_.offset, cmdMem_.size);
        return true;
    }

    ShaderHandle CreateShader(const ShaderDesc& desc) override
    {
        std::string path = desc.source;
        if (path.empty())
        {
            path = desc.stage == ShaderStage::Vertex ? "shaders/deko_triangle_vsh.dksh" : "shaders/deko_triangle_fsh.dksh";
        }
        else if (path.rfind("romfs:/", 0) != 0)
        {
            path = FileSystem::ResolvePath(path);
        }

        DkShaderResource shader;
        shader.stage = desc.stage;
        if (!LoadShaderBlob(path, shader.blob))
        {
            return {};
        }

        shaders_.push_back(std::move(shader));
        return ShaderHandle{static_cast<std::uint32_t>(shaders_.size())};
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override
    {
        if (Lookup(shaders_, desc.vertexShader.id) == nullptr ||
            Lookup(shaders_, desc.fragmentShader.id) == nullptr ||
            desc.vertexLayout.stride == 0)
        {
            return {};
        }

        DkPipelineResource pipeline;
        pipeline.vertexShader = desc.vertexShader;
        pipeline.fragmentShader = desc.fragmentShader;
        pipeline.topology = desc.topology;
        pipeline.attributes.assign(desc.vertexLayout.attributes, desc.vertexLayout.attributes + desc.vertexLayout.attributeCount);
        pipeline.vertexLayout = desc.vertexLayout;
        pipeline.vertexLayout.attributes = pipeline.attributes.data();
        pipelines_.push_back(std::move(pipeline));
        return PipelineHandle{static_cast<std::uint32_t>(pipelines_.size())};
    }

    CommandBufferHandle CreateCommandBuffer() override
    {
        commandBuffers_.push_back(DkCommandResource{});
        return CommandBufferHandle{static_cast<std::uint32_t>(commandBuffers_.size())};
    }

    void DestroyBuffer(BufferHandle handle) override
    {
        if (auto* buffer = Lookup(buffers_, handle.id))
        {
            buffer->allocation = {};
        }
    }

    void DestroyTexture(TextureHandle handle) override
    {
        if (auto* texture = Lookup(textures_, handle.id))
        {
            *texture = {};
        }
    }

    void DestroyShader(ShaderHandle handle) override
    {
        if (auto* shader = Lookup(shaders_, handle.id))
        {
            shader->blob = {};
        }
    }

    void DestroyPipeline(PipelineHandle handle) override
    {
        if (auto* pipeline = Lookup(pipelines_, handle.id))
        {
            *pipeline = {};
        }
    }

    void DestroyCommandBuffer(CommandBufferHandle handle) override
    {
        if (auto* commandBuffer = Lookup(commandBuffers_, handle.id))
        {
            *commandBuffer = {};
        }
    }

    void BeginFrame(SwapchainHandle swapchain, const RenderPassDesc& renderPass) override
    {
        (void)swapchain;
        currentClearColor_ = renderPass.clearColor;
        const int slot = queue_.acquireImage(swapchain_);
        currentFramebuffer_ = static_cast<std::uint32_t>(slot);
    }

    void BeginCommandBuffer(CommandBufferHandle commandBuffer) override
    {
        if (auto* command = Lookup(commandBuffers_, commandBuffer.id))
        {
            *command = {};
        }
        cmdBuf_.clear();
        cmdBuf_.addMemory(cmdMem_.block, cmdMem_.offset, cmdMem_.size);
        dk::ImageView colorTarget{ framebuffers_[currentFramebuffer_] };
        dk::ImageView depthTarget{ depthBuffer_ };
        cmdBuf_.bindRenderTargets(&colorTarget, &depthTarget);
        cmdBuf_.setViewports(0, { { 0.0f, 0.0f, framebufferWidth_, framebufferHeight_, 0.0f, 1.0f } });
        cmdBuf_.setScissors(0, { { 0, 0, static_cast<std::uint32_t>(framebufferWidth_), static_cast<std::uint32_t>(framebufferHeight_) } });
        cmdBuf_.clearColor(0, DkColorMask_RGBA, currentClearColor_.r, currentClearColor_.g, currentClearColor_.b, currentClearColor_.a);
        dk::RasterizerState rasterizerState;
        dk::ColorState colorState;
        dk::ColorWriteState colorWriteState;
        cmdBuf_.bindRasterizerState(rasterizerState);
        cmdBuf_.bindColorState(colorState);
        cmdBuf_.bindColorWriteState(colorWriteState);
    }

    void BindPipeline(CommandBufferHandle commandBuffer, PipelineHandle pipeline) override
    {
        DkCommandResource* command = Lookup(commandBuffers_, commandBuffer.id);
        DkPipelineResource* pipelineResource = Lookup(pipelines_, pipeline.id);
        if (command == nullptr || pipelineResource == nullptr)
        {
            return;
        }

        command->pipeline = pipeline;
        DkShaderResource* vertexShader = Lookup(shaders_, pipelineResource->vertexShader.id);
        DkShaderResource* fragmentShader = Lookup(shaders_, pipelineResource->fragmentShader.id);
        if (vertexShader == nullptr || fragmentShader == nullptr)
        {
            return;
        }

        std::array<DkShader const*, 2> shaders = {{
            &vertexShader->blob.shader,
            &fragmentShader->blob.shader,
        }};
        cmdBuf_.bindShaders(DkStageFlag_GraphicsMask, shaders);
        BuildVertexState(*pipelineResource);
        cmdBuf_.bindVtxAttribState({static_cast<std::uint32_t>(vertexAttribState_.size()), vertexAttribState_.data()});
        cmdBuf_.bindVtxBufferState(vertexBufferState_);
    }

    void BindVertexBuffer(CommandBufferHandle commandBuffer, BufferHandle buffer) override
    {
        DkCommandResource* command = Lookup(commandBuffers_, commandBuffer.id);
        DkBufferResource* bufferResource = Lookup(buffers_, buffer.id);
        if (command == nullptr || bufferResource == nullptr)
        {
            return;
        }

        command->vertexBuffer = buffer;
        cmdBuf_.bindVtxBuffer(0, bufferResource->allocation.gpu, static_cast<std::uint32_t>(bufferResource->allocation.size));
    }

    void BindTexture(CommandBufferHandle commandBuffer, TextureHandle texture) override
    {
        DkCommandResource* command = Lookup(commandBuffers_, commandBuffer.id);
        DkTextureResource* textureResource = Lookup(textures_, texture.id);
        if (command == nullptr || textureResource == nullptr)
        {
            return;
        }

        command->texture = texture;
        cmdBuf_.bindImageDescriptorSet(textureDescriptorMemory_.gpu, kTextureDescriptorCount);
        cmdBuf_.bindSamplerDescriptorSet(samplerDescriptorMemory_.gpu, kTextureDescriptorCount);
        const DkResHandle handle = dkMakeTextureHandle(textureResource->imageDescriptorIndex, textureResource->samplerDescriptorIndex);
        cmdBuf_.bindTextures(DkStage_Fragment, 0, {1, &handle});
    }

    void Draw(CommandBufferHandle commandBuffer, std::uint32_t vertexCount, std::uint32_t firstVertex) override
    {
        if (auto* command = Lookup(commandBuffers_, commandBuffer.id))
        {
            command->vertexCount = vertexCount;
            command->firstVertex = firstVertex;
        }

        cmdBuf_.draw(DkPrimitive_Triangles, vertexCount, 1, firstVertex, 0);
    }

    void EndCommandBuffer(CommandBufferHandle commandBuffer) override
    {
        (void)commandBuffer;
        renderCmdList_ = cmdBuf_.finishList();
    }

    void Submit(CommandBufferHandle commandBuffer) override
    {
        (void)commandBuffer;
        queue_.submitCommands(renderCmdList_);
        renderCmdList_ = {};
    }

    void EndFrame(SwapchainHandle swapchain) override
    {
        (void)swapchain;
        queue_.presentImage(swapchain_, currentFramebuffer_);
    }

    const char* BackendName() const override
    {
        return "Deko3D";
    }

private:
    static DkVtxAttribSize ToDkAttribSize(VertexFormat format)
    {
        switch (format)
        {
        case VertexFormat::Float2:
            return DkVtxAttribSize_2x32;
        case VertexFormat::Float3:
            return DkVtxAttribSize_3x32;
        case VertexFormat::Float4:
            return DkVtxAttribSize_4x32;
        }

        return DkVtxAttribSize_2x32;
    }

    bool LoadShaderBlob(const std::string& path, ShaderBlob& blob)
    {
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
        {
            std::fprintf(stderr, "failed to open shader: %s\n", path.c_str());
            return false;
        }

        DkShaderBinaryHeader header{};
        if (std::fread(&header, sizeof(header), 1, file) != 1)
        {
            std::fclose(file);
            return false;
        }

        blob.control.resize(header.controlSize);
        std::vector<std::uint8_t> code(header.codeSize);
        std::rewind(file);
        const bool ok =
            std::fread(blob.control.data(), header.controlSize, 1, file) == 1 &&
            std::fread(code.data(), header.codeSize, 1, file) == 1;
        std::fclose(file);
        if (!ok)
        {
            return false;
        }

        blob.codeMem = codePool_->allocate(code.size(), DK_SHADER_CODE_ALIGNMENT);
        if (!blob.codeMem)
        {
            return false;
        }

        std::memcpy(blob.codeMem.cpu, code.data(), code.size());
        dk::ShaderMaker{blob.codeMem.block, static_cast<std::uint32_t>(blob.codeMem.offset)}
            .setControl(blob.control.data())
            .setProgramId(0)
            .initialize(blob.shader);
        return true;
    }

    void BuildVertexState(const DkPipelineResource& pipeline)
    {
        vertexAttribState_.clear();
        for (const VertexAttributeDesc& attribute : pipeline.attributes)
        {
            vertexAttribState_.push_back(DkVtxAttribState{
                0,
                0,
                static_cast<std::uint32_t>(attribute.offset),
                ToDkAttribSize(attribute.format),
                DkVtxAttribType_Float,
                0,
            });
        }

        vertexBufferState_[0] = DkVtxBufferState{static_cast<std::uint32_t>(pipeline.vertexLayout.stride), 0};
    }

    bool CreateFramebufferResources()
    {
        dk::ImageLayout framebufferLayout;
        dk::ImageLayoutMaker{device_}
            .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_Usage2DEngine)
            .setFormat(DkImageFormat_RGBA8_Unorm)
            .setDimensions(framebufferWidth_, framebufferHeight_)
            .initialize(framebufferLayout);

        dk::ImageLayout depthLayout;
        dk::ImageLayoutMaker{device_}
            .setFlags(DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine)
            .setFormat(DkImageFormat_S8)
            .setDimensions(framebufferWidth_, framebufferHeight_)
            .initialize(depthLayout);

        depthBufferMem_ = imagePool_->allocate(depthLayout.getSize(), depthLayout.getAlignment());
        if (!depthBufferMem_)
        {
            std::fprintf(stderr, "failed to allocate Deko3D depth buffer\n");
            return false;
        }
        depthBuffer_.initialize(depthLayout, depthBufferMem_.block, depthBufferMem_.offset);

        std::array<DkImage const*, kFramebufferCount> fbArray{};
        for (std::uint32_t i = 0; i < kFramebufferCount; ++i)
        {
            framebufferMem_[i] = imagePool_->allocate(framebufferLayout.getSize(), framebufferLayout.getAlignment());
            if (!framebufferMem_[i])
            {
                std::fprintf(stderr, "failed to allocate Deko3D framebuffer %u\n", i);
                return false;
            }
            framebuffers_[i].initialize(framebufferLayout, framebufferMem_[i].block, framebufferMem_[i].offset);
            fbArray[i] = &framebuffers_[i];
        }

        swapchain_ = dk::SwapchainMaker{device_, nwindowGetDefault(), fbArray}.create();
        return static_cast<bool>(swapchain_);
    }

    void DestroyFramebufferResources()
    {
        if (!swapchain_)
        {
            return;
        }

        for (auto& mem : framebufferMem_)
        {
            mem = {};
        }
        depthBufferMem_ = {};
        swapchain_.destroy();
    }

    dk::Device device_{};
    dk::Queue queue_{};
    dk::UniqueCmdBuf cmdBuf_{};
    dk::UniqueSwapchain swapchain_{};
    std::optional<LinearPool> imagePool_;
    std::optional<LinearPool> codePool_;
    std::optional<LinearPool> dataPool_;
    std::vector<DkBufferResource> buffers_;
    std::vector<DkTextureResource> textures_;
    std::vector<DkShaderResource> shaders_;
    std::vector<DkPipelineResource> pipelines_;
    std::vector<DkCommandResource> commandBuffers_;
    dk::Image framebuffers_[kFramebufferCount]{};
    LinearPool::Allocation framebufferMem_[kFramebufferCount]{};
    dk::Image depthBuffer_{};
    LinearPool::Allocation depthBufferMem_{};
    LinearPool::Allocation cmdMem_{};
    LinearPool::Allocation textureDescriptorMemory_{};
    LinearPool::Allocation samplerDescriptorMemory_{};
    std::uint32_t nextTextureDescriptor_ = 0;
    std::vector<DkVtxAttribState> vertexAttribState_;
    std::array<DkVtxBufferState, 1> vertexBufferState_{};
    DkCmdList renderCmdList_{};
    std::uint32_t currentFramebuffer_ = 0;
    Color currentClearColor_{};
    float framebufferWidth_ = 1280.0f;
    float framebufferHeight_ = 720.0f;
};
}

std::unique_ptr<Device> CreateDeko3DDevice(Platform& platform)
{
    return std::make_unique<Deko3DDevice>(platform);
}

}
