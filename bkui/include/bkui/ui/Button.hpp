#pragma once

#include <bkui/ui/View.hpp>

#include <string>

namespace bk
{

class Button final : public View
{
public:
    explicit Button(std::string text = {});

    void SetText(std::string text);
    [[nodiscard]] const std::string& GetText() const;

    void SetBackgroundColor(const Color& color);
    [[nodiscard]] const Color& GetBackgroundColor() const;

    void SetTextColor(const Color& color);
    [[nodiscard]] const Color& GetTextColor() const;

    Size Measure(const Size& available) const override;
    void Render(RenderQueue& queue) const override;

private:
    std::string text_;
    Color backgroundColor_{0.18F, 0.45F, 0.90F, 1.0F};
    Color textColor_{1.0F, 1.0F, 1.0F, 1.0F};
};

}
