#pragma once

#include <bkui/core/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace bk
{

struct BufferHandle
{
    std::uint32_t id = 0;
};

struct TextureHandle
{
    std::uint32_t id = 0;
};

struct ShaderHandle
{
    std::uint32_t id = 0;
};

struct PipelineHandle
{
    std::uint32_t id = 0;
};

struct CommandBufferHandle
{
    std::uint32_t id = 0;
};

struct SwapchainHandle
{
    std::uint32_t id = 0;
};

enum class BufferUsage
{
    Vertex,
    Index,
    Uniform,
};

enum class ShaderStage
{
    Vertex,
    Fragment,
};

enum class VertexFormat
{
    Float2,
    Float3,
    Float4,
};

enum class PrimitiveTopology
{
    Triangles,
};

struct BufferDesc
{
    BufferUsage usage = BufferUsage::Vertex;
    std::size_t size = 0;
    const void* data = nullptr;
};

struct TextureDesc
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const void* rgba = nullptr;
};

struct ShaderDesc
{
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
};

struct VertexAttributeDesc
{
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float2;
    std::size_t offset = 0;
};

struct VertexLayoutDesc
{
    std::size_t stride = 0;
    const VertexAttributeDesc* attributes = nullptr;
    std::size_t attributeCount = 0;
};

struct PipelineDesc
{
    ShaderHandle vertexShader;
    ShaderHandle fragmentShader;
    VertexLayoutDesc vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

struct RenderPassDesc
{
    Color clearColor;
};

class Device
{
public:
    virtual ~Device() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Resize(Vector2 size) = 0;

    virtual SwapchainHandle GetMainSwapchain() const = 0;
    virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
    virtual bool UpdateBuffer(BufferHandle handle, const BufferDesc& desc) = 0;
    virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
    virtual bool UpdateTexture(TextureHandle handle, const TextureDesc& desc) = 0;
    virtual ShaderHandle CreateShader(const ShaderDesc& desc) = 0;
    virtual PipelineHandle CreatePipeline(const PipelineDesc& desc) = 0;
    virtual CommandBufferHandle CreateCommandBuffer() = 0;

    virtual void DestroyBuffer(BufferHandle handle) = 0;
    virtual void DestroyTexture(TextureHandle handle) = 0;
    virtual void DestroyShader(ShaderHandle handle) = 0;
    virtual void DestroyPipeline(PipelineHandle handle) = 0;
    virtual void DestroyCommandBuffer(CommandBufferHandle handle) = 0;

    virtual void BeginFrame(SwapchainHandle swapchain, const RenderPassDesc& renderPass) = 0;
    virtual void BeginCommandBuffer(CommandBufferHandle commandBuffer) = 0;
    virtual void BindPipeline(CommandBufferHandle commandBuffer, PipelineHandle pipeline) = 0;
    virtual void BindVertexBuffer(CommandBufferHandle commandBuffer, BufferHandle buffer) = 0;
    virtual void BindTexture(CommandBufferHandle commandBuffer, TextureHandle texture) = 0;
    virtual void Draw(CommandBufferHandle commandBuffer, std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
    virtual void EndCommandBuffer(CommandBufferHandle commandBuffer) = 0;
    virtual void Submit(CommandBufferHandle commandBuffer) = 0;
    virtual void EndFrame(SwapchainHandle swapchain) = 0;

    virtual const char* BackendName() const = 0;
};

inline bool IsValid(BufferHandle handle)
{
    return handle.id != 0;
}

inline bool IsValid(ShaderHandle handle)
{
    return handle.id != 0;
}

inline bool IsValid(TextureHandle handle)
{
    return handle.id != 0;
}

inline bool IsValid(PipelineHandle handle)
{
    return handle.id != 0;
}

inline bool IsValid(CommandBufferHandle handle)
{
    return handle.id != 0;
}

inline bool IsValid(SwapchainHandle handle)
{
    return handle.id != 0;
}

}
