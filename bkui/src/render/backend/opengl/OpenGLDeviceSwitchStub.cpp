#include <bkui/render/backend/opengl/OpenGLDevice.hpp>

#include <memory>

namespace bk
{
namespace
{
class OpenGLDeviceSwitchStub final : public Device
{
public:
    explicit OpenGLDeviceSwitchStub(Platform& platform)
        : platform_(platform)
    {
    }

    bool Init() override
    {
        return false;
    }

    void Shutdown() override {}
    void Resize(Vector2 size) override { (void)size; }

    SwapchainHandle GetMainSwapchain() const override
    {
        return SwapchainHandle{1};
    }

    BufferHandle CreateBuffer(const BufferDesc& desc) override
    {
        (void)desc;
        return BufferHandle{++nextHandle_};
    }

    ShaderHandle CreateShader(const ShaderDesc& desc) override
    {
        (void)desc;
        return ShaderHandle{++nextHandle_};
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override
    {
        (void)desc;
        return PipelineHandle{++nextHandle_};
    }

    CommandBufferHandle CreateCommandBuffer() override
    {
        return CommandBufferHandle{++nextHandle_};
    }

    void DestroyBuffer(BufferHandle handle) override { (void)handle; }
    void DestroyShader(ShaderHandle handle) override { (void)handle; }
    void DestroyPipeline(PipelineHandle handle) override { (void)handle; }
    void DestroyCommandBuffer(CommandBufferHandle handle) override { (void)handle; }
    void BeginFrame(SwapchainHandle swapchain, const RenderPassDesc& renderPass) override
    {
        (void)swapchain;
        (void)renderPass;
    }
    void BeginCommandBuffer(CommandBufferHandle commandBuffer) override { (void)commandBuffer; }
    void BindPipeline(CommandBufferHandle commandBuffer, PipelineHandle pipeline) override
    {
        (void)commandBuffer;
        (void)pipeline;
    }
    void BindVertexBuffer(CommandBufferHandle commandBuffer, BufferHandle buffer) override
    {
        (void)commandBuffer;
        (void)buffer;
    }
    void Draw(CommandBufferHandle commandBuffer, std::uint32_t vertexCount, std::uint32_t firstVertex) override
    {
        (void)commandBuffer;
        (void)vertexCount;
        (void)firstVertex;
    }
    void EndCommandBuffer(CommandBufferHandle commandBuffer) override { (void)commandBuffer; }
    void Submit(CommandBufferHandle commandBuffer) override { (void)commandBuffer; }

    void EndFrame(SwapchainHandle swapchain) override
    {
        (void)swapchain;
        platform_.SwapOpenGLBuffers();
    }

    const char* BackendName() const override
    {
        return "OpenGL unavailable on this Switch bootstrap build";
    }

private:
    Platform& platform_;
    std::uint32_t nextHandle_ = 1;
};
}

std::unique_ptr<Device> CreateOpenGLDevice(Platform& platform)
{
    return std::make_unique<OpenGLDeviceSwitchStub>(platform);
}

}
