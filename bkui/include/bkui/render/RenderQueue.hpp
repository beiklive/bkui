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
    void PushRect(const Rect& bounds, const Color& color);
    /// 压入一条文本绘制命令。
    void PushText(const Rect& bounds, std::string text, float fontSize, const Color& color);

    /// 只读访问当前收集到的命令列表。
    [[nodiscard]] const std::vector<RenderCommand>& Commands() const;

private:
    std::vector<RenderCommand> commands_;
};

}
