#pragma once

#include <bkui/render/RenderCommand.hpp>

#include <vector>

namespace bk
{

/// 用于收集一帧 UI 绘制命令的轻量队列。
class RenderQueue
{
public:
    /// 清空当前队列中的全部命令。
    void Clear();
    /// 压入一个矩形填充命令。
    void PushRect(const Rect& bounds, const ColorRGBA& color);
    /// 压入一个圆角矩形填充命令。
    void PushRoundedRect(const Rect& bounds, const ColorRGBA& color, float cornerRadius);
    /// 压入一条文本绘制命令。
    void PushText(const Rect& bounds, std::string text, float fontSize, const ColorRGBA& color);
    /// 压入一条线段命令。
    void PushLine(const Vector2& start, const Vector2& end, const ColorRGBA& color, float thickness = 1.5F);
    /// 开始一个新的裁剪区域。
    void PushClipRect(const Rect& bounds);
    /// 结束当前裁剪区域。
    void PopClipRect();
    /// 开始一个缩放变换（累积到当前变换栈）。
    void PushTransform(float scale, float pivotX, float pivotY);
    /// 结束当前缩放变换。
    void PopTransform();

    /// 只读访问当前收集到的命令列表。
    [[nodiscard]] const std::vector<RenderCommand>& Commands() const;

private:
    std::vector<RenderCommand> commands_;
};

}
