#pragma once

#include <bkui/render/Device.hpp>
#include <bkui/render/RenderQueue.hpp>

namespace bk
{

/// 将 RenderQueue 直接转换为 GPU 绘制命令的轻量渲染器。
class RenderQueueRenderer
{
public:
    explicit RenderQueueRenderer(Device& device);
    ~RenderQueueRenderer();

    RenderQueueRenderer(const RenderQueueRenderer&) = delete;
    RenderQueueRenderer& operator=(const RenderQueueRenderer&) = delete;

    /// 初始化渲染器内部使用的纹理、着色器和管线资源。
    [[nodiscard]] bool Initialize();
    /// 释放渲染器创建的 GPU 资源。
    void Shutdown();

    /// 将一帧 RenderQueue 记录为绘制命令。
    /// logicalSize 表示 UI 布局坐标系，targetSize 表示最终输出像素尺寸。
    [[nodiscard]] bool Render(CommandBufferHandle commandBuffer, const RenderQueue& queue, Vector2 logicalSize, Vector2 targetSize);

private:
    class Impl;

    Device& device_;
    Impl* impl_ = nullptr;
};

}
