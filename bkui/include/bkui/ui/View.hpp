#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/renderer/RenderQueue.hpp>

#include <memory>
#include <vector>

namespace bk
{

struct Size
{
    float width = 0.0F;
    float height = 0.0F;
};

class View
{
public:
    virtual ~View() = default;

    void SetFrame(const Rect& frame);
    [[nodiscard]] const Rect& GetFrame() const;

    void AddChild(std::shared_ptr<View> child);
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& Children() const;

    virtual Size Measure(const Size& available) const;
    virtual void Layout();
    virtual void Render(RenderQueue& queue) const;

protected:
    Rect frame_{};
    std::vector<std::shared_ptr<View>> children_;
};

}
