#pragma once

#include <bkui/ui/View.hpp>

namespace bk
{

enum class BoxDirection
{
    Horizontal,
    Vertical,
};

class Box : public View
{
public:
    explicit Box(BoxDirection direction);

    void SetSpacing(float spacing);
    [[nodiscard]] float GetSpacing() const;

    Size Measure(const Size& available) const override;
    void Layout() override;

protected:
    BoxDirection direction_;
    float spacing_ = 0.0F;
};

class HBox final : public Box
{
public:
    HBox();
};

class VBox final : public Box
{
public:
    VBox();
};

}
