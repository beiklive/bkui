#pragma once

#include <bkui/ui/View.hpp>

#include <string>

namespace bk
{

class Label final : public View
{
public:
    explicit Label(std::string text = {});

    void SetText(std::string text);
    [[nodiscard]] const std::string& GetText() const;

    void SetFontSize(float fontSize);
    [[nodiscard]] float GetFontSize() const;

    void SetTextColor(const Color& color);
    [[nodiscard]] const Color& GetTextColor() const;

    Size Measure(const Size& available) const override;
    void Render(RenderQueue& queue) const override;

private:
    std::string text_;
    float fontSize_ = 20.0F;
    Color textColor_{0.1F, 0.1F, 0.1F, 1.0F};
};

}
