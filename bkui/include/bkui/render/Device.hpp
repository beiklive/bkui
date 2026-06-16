#pragma once

#include <bkui/core/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace bk
{

/// GPU 缓冲区句柄。
struct BufferHandle
{
    std::uint32_t id = 0;
};

/// 纹理句柄。
struct TextureHandle
{
    std::uint32_t id = 0;
};

/// 着色器句柄。
struct ShaderHandle
{
    std::uint32_t id = 0;
};

/// 管线对象句柄。
struct PipelineHandle
{
    std::uint32_t id = 0;
};

/// 命令缓冲句柄。
struct CommandBufferHandle
{
    std::uint32_t id = 0;
};

/// 交换链句柄。
struct SwapchainHandle
{
    std::uint32_t id = 0;
};

/// 缓冲区用途。
enum class BufferUsage
{
    Vertex,
    Index,
    Uniform,
};

/// 着色器阶段。
enum class ShaderStage
{
    Vertex,
    Fragment,
};

/// 顶点属性格式。
enum class VertexFormat
{
    Float2,
    Float3,
    Float4,
};

/// 图元拓扑类型。
enum class PrimitiveTopology
{
    Triangles,
};

/// 缓冲区创建或更新参数。
struct BufferDesc
{
    BufferUsage usage = BufferUsage::Vertex;
    std::size_t size = 0;
    const void* data = nullptr;
};

/// 纹理创建或更新参数，当前默认使用 RGBA8 数据。
struct TextureDesc
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const void* rgba = nullptr;
};

/// 纹理局部更新参数，坐标和尺寸以像素为单位。
struct TextureRegionDesc
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const void* rgba = nullptr;
};

/// 着色器创建参数。
struct ShaderDesc
{
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
};

/// 单个顶点属性描述。
struct VertexAttributeDesc
{
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float2;
    std::size_t offset = 0;
};

/// 顶点布局描述。
struct VertexLayoutDesc
{
    std::size_t stride = 0;
    const VertexAttributeDesc* attributes = nullptr;
    std::size_t attributeCount = 0;
};

/// 图形管线创建参数。
struct PipelineDesc
{
    ShaderHandle vertexShader;
    ShaderHandle fragmentShader;
    VertexLayoutDesc vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

/// 一次渲染通道的基础参数。
struct RenderPassDesc
{
    Color clearColor;
};

/// 渲染硬件接口抽象，屏蔽不同图形后端的实现差异。
class Device
{
public:
    virtual ~Device() = default;

    /// 初始化图形设备。
    virtual bool Init() = 0;
    /// 关闭图形设备并释放资源。
    virtual void Shutdown() = 0;
    /// 响应窗口尺寸变化。
    virtual void Resize(Vector2 size) = 0;

    /// 获取主交换链句柄。
    virtual SwapchainHandle GetMainSwapchain() const = 0;
    /// 创建 GPU 缓冲区。
    virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
    /// 更新已有 GPU 缓冲区内容。
    virtual bool UpdateBuffer(BufferHandle handle, const BufferDesc& desc) = 0;
    /// 创建纹理资源。
    virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
    /// 更新已有纹理内容。
    virtual bool UpdateTexture(TextureHandle handle, const TextureDesc& desc) = 0;
    /// 更新已有纹理的局部区域。
    virtual bool UpdateTextureRegion(TextureHandle handle, const TextureRegionDesc& desc) = 0;
    /// 创建着色器对象。
    virtual ShaderHandle CreateShader(const ShaderDesc& desc) = 0;
    /// 创建图形管线对象。
    virtual PipelineHandle CreatePipeline(const PipelineDesc& desc) = 0;
    /// 创建命令缓冲。
    virtual CommandBufferHandle CreateCommandBuffer() = 0;

    /// 销毁 GPU 缓冲区。
    virtual void DestroyBuffer(BufferHandle handle) = 0;
    /// 销毁纹理资源。
    virtual void DestroyTexture(TextureHandle handle) = 0;
    /// 销毁着色器对象。
    virtual void DestroyShader(ShaderHandle handle) = 0;
    /// 销毁图形管线对象。
    virtual void DestroyPipeline(PipelineHandle handle) = 0;
    /// 销毁命令缓冲。
    virtual void DestroyCommandBuffer(CommandBufferHandle handle) = 0;

    /// 开始一帧渲染。
    virtual void BeginFrame(SwapchainHandle swapchain, const RenderPassDesc& renderPass) = 0;
    /// 开始录制命令缓冲。
    virtual void BeginCommandBuffer(CommandBufferHandle commandBuffer) = 0;
    /// 绑定图形管线。
    virtual void BindPipeline(CommandBufferHandle commandBuffer, PipelineHandle pipeline) = 0;
    /// 绑定顶点缓冲。
    virtual void BindVertexBuffer(CommandBufferHandle commandBuffer, BufferHandle buffer) = 0;
    /// 绑定纹理。
    virtual void BindTexture(CommandBufferHandle commandBuffer, TextureHandle texture) = 0;
    /// 发起一次非索引绘制。
    virtual void Draw(CommandBufferHandle commandBuffer, std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
    /// 结束命令录制。
    virtual void EndCommandBuffer(CommandBufferHandle commandBuffer) = 0;
    /// 提交命令缓冲到 GPU 执行。
    virtual void Submit(CommandBufferHandle commandBuffer) = 0;
    /// 结束当前帧并提交显示。
    virtual void EndFrame(SwapchainHandle swapchain) = 0;

    /// 返回当前后端名称，便于日志或调试显示。
    virtual const char* BackendName() const = 0;
};

/// 判断缓冲区句柄是否有效。
inline bool IsValid(BufferHandle handle)
{
    return handle.id != 0;
}

/// 判断着色器句柄是否有效。
inline bool IsValid(ShaderHandle handle)
{
    return handle.id != 0;
}

/// 判断纹理句柄是否有效。
inline bool IsValid(TextureHandle handle)
{
    return handle.id != 0;
}

/// 判断管线句柄是否有效。
inline bool IsValid(PipelineHandle handle)
{
    return handle.id != 0;
}

/// 判断命令缓冲句柄是否有效。
inline bool IsValid(CommandBufferHandle handle)
{
    return handle.id != 0;
}

/// 判断交换链句柄是否有效。
inline bool IsValid(SwapchainHandle handle)
{
    return handle.id != 0;
}

}
