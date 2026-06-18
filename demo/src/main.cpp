#include <bkui/bkui.hpp>

#include <cstdio>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float kDesignWidth = 1280.0F;
constexpr float kDesignHeight = 720.0F;

std::string FormatPercent(float value)
{
    std::ostringstream stream;
    stream << static_cast<int>(value * 100.0F + 0.5F) << "%";
    return stream.str();
}

std::string FormatNumber(float value, int precision = 2)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(precision);
    stream << value;
    return stream.str();
}

class ColorSwatchRow final : public bk::Box
{
public:
    ColorSwatchRow()
        : bk::Box(bk::BoxDirection::Horizontal)
    {
        SetSpacing(12.0F);
        SetHeight(30.0F);
        SetMinHeight(30.0F);
    }

    void SetPalette(const std::vector<bk::ColorRGBA>& colors)
    {
        ClearChildren();
        for (const bk::ColorRGBA& color : colors)
        {
            auto swatch = std::make_shared<bk::View>();
            swatch->SetWidth(30.0F);
            swatch->SetHeight(30.0F);
            swatch->SetCornerRadius(8.0F);
            swatch->SetDrawBackground(true);
            swatch->SetBackgroundColor(color);
            AddChild(swatch);
        }
    }
};

class ControlsShowcasePage final : public bk::Box
{
public:
    ControlsShowcasePage()
        : bk::Box(bk::BoxDirection::Vertical)
    {
        SetName("ControlsShowcasePage");
        SetDrawBackground(true);
        SetBackgroundColor(bk::ColorRGBA(18, 22, 30, 255));

        pageScrollBox_ = std::make_shared<bk::ScrollBox>(bk::ScrollAxis::Vertical);
        pageScrollBox_->SetFlexGrow(1.0F);
        pageScrollBox_->SetDrawBackground(false);
        pageScrollBox_->SetPadding(24.0F);
        pageScrollBox_->SetScrollBarThickness(10.0F);
        pageScrollBox_->SetScrollBarTrackColor(bk::ColorRGBA(10, 13, 18, 132));
        pageScrollBox_->SetScrollBarThumbColor(bk::ColorRGBA(176, 188, 208, 230));

        pageContent_ = std::make_shared<bk::VBox>();
        pageContent_->SetSpacing(18.0F);
        pageScrollBox_->SetContent(pageContent_);
        AddChild(pageScrollBox_);

        BuildHeader();
        BuildBody();
        WireEvents();
        RefreshStateTexts();
    }

private:
    void BuildHeader()
    {
        auto hero = std::make_shared<bk::VBox>();
        hero->SetSpacing(10.0F);
        hero->SetPadding(18.0F);
        hero->SetDrawBackground(true);
        hero->SetBackgroundColor(bk::ColorRGBA(27, 33, 44, 255));
        hero->SetCornerRadius(12.0F);
        hero->SetShadowEnabled(true);
        hero->SetShadowColor(bk::ColorRGBA(0, 0, 0, 96));
        hero->SetShadowBlurRadius(20.0F);
        hero->SetShadowOffset(0.0F, 8.0F);

        auto title = std::make_shared<bk::Label>("BeikUI Controls Demo");
        title->SetFontSize(30.0F);
        title->SetTextColor(bk::ColorRGBA(242, 246, 255, 255));

        auto subtitle = std::make_shared<bk::Label>(
            "展示常用控件的属性、接口和回调，F1 可切换日志 Overlay。");
        subtitle->SetFontSize(16.0F);
        subtitle->SetTextColor(bk::ColorRGBA(155, 166, 184, 255));

        auto swatches = std::make_shared<ColorSwatchRow>();
        swatches->SetPalette({
            bk::ColorRGBA(64, 163, 255, 255),
            bk::ColorRGBA(88, 214, 141, 255),
            bk::ColorRGBA(255, 195, 74, 255),
            bk::ColorRGBA(255, 122, 122, 255),
            bk::ColorRGBA(179, 124, 255, 255),
        });

        hero->AddChild(title);
        hero->AddChild(subtitle);
        hero->AddChild(swatches);
        pageContent_->AddChild(hero);
    }

    void BuildBody()
    {
        auto columns = std::make_shared<bk::HBox>();
        columns->SetSpacing(18.0F);
        columns->SetFlexGrow(1.0F);
        columns->SetHeightPercentage(1.0F);

        auto leftColumn = std::make_shared<bk::VBox>();
        leftColumn->SetSpacing(18.0F);
        leftColumn->SetFlexGrow(1.0F);

        auto rightColumn = std::make_shared<bk::VBox>();
        rightColumn->SetSpacing(18.0F);
        rightColumn->SetWidth(360.0F);

        leftColumn->AddChild(BuildActionsCard());
        leftColumn->AddChild(BuildSettingsCard());
        leftColumn->AddChild(BuildScrollPlaygroundCard());

        rightColumn->AddChild(BuildStateCard());
        rightColumn->AddChild(BuildPreviewCard());

        columns->AddChild(leftColumn);
        columns->AddChild(rightColumn);
        pageContent_->AddChild(columns);
    }

    std::shared_ptr<bk::View> BuildActionsCard()
    {
        auto card = MakeCard("Button");
        auto row = std::make_shared<bk::HBox>();
        row->SetSpacing(12.0F);

        primaryButton_ = std::make_shared<bk::Button>("Primary");
        primaryButton_->SetCornerRadius(10.0F);
        primaryButton_->SetPadding(12.0F, 18.0F);
        primaryButton_->SetPressedBackgroundColor(bk::ColorRGBA(24, 104, 214, 255));
        primaryButton_->SetFocusHighlightColors(
            bk::ColorRGBA(108, 196, 255, 255),
            bk::ColorRGBA(116, 132, 255, 255));

        secondaryButton_ = std::make_shared<bk::Button>("Secondary");
        secondaryButton_->SetCornerRadius(10.0F);
        secondaryButton_->SetBackgroundColor(bk::ColorRGBA(56, 67, 86, 255));
        secondaryButton_->SetPressedBackgroundColor(bk::ColorRGBA(43, 52, 67, 255));
        secondaryButton_->SetPadding(12.0F, 18.0F);

        disabledButton_ = std::make_shared<bk::Button>("Disabled");
        disabledButton_->SetCornerRadius(10.0F);
        disabledButton_->SetEnabled(false);
        disabledButton_->SetPadding(12.0F, 18.0F);

        row->AddChild(primaryButton_);
        row->AddChild(secondaryButton_);
        row->AddChild(disabledButton_);

        card->AddChild(row);
        return card;
    }

    std::shared_ptr<bk::View> BuildSettingsCard()
    {
        auto card = MakeCard("Inputs");

        soundCheckBox_ = std::make_shared<bk::CheckBox>("Enable sound", true);
        compactCheckBox_ = std::make_shared<bk::CheckBox>("Compact mode", false);

        intensityLabel_ = std::make_shared<bk::Label>();
        intensityLabel_->SetFontSize(15.0F);
        intensityLabel_->SetTextColor(bk::ColorRGBA(188, 197, 214, 255));

        intensitySlider_ = std::make_shared<bk::Slider>(0.0F, 1.0F, 0.42F);
        intensitySlider_->SetStep(0.01F);
        intensitySlider_->SetHeight(34.0F);

        progressBar_ = std::make_shared<bk::ProgressBar>(0.0F, 1.0F, 0.42F);
        progressBar_->SetHeight(24.0F);
        progressBar_->SetCornerRadius(10.0F);

        card->AddChild(soundCheckBox_);
        card->AddChild(compactCheckBox_);
        card->AddChild(intensityLabel_);
        card->AddChild(intensitySlider_);
        card->AddChild(progressBar_);
        return card;
    }

    std::shared_ptr<bk::View> BuildScrollPlaygroundCard()
    {
        auto card = MakeCard("ScrollBox Playground");
        auto hint = std::make_shared<bk::Label>(
            "支持滚轮、拖动滚动条、单指/双指/三指滑动。单指轻扫即可触发惯性。");
        hint->SetFontSize(14.0F);
        hint->SetTextColor(bk::ColorRGBA(150, 161, 179, 255));

        auto sections = std::make_shared<bk::VBox>();
        sections->SetSpacing(14.0F);

        verticalScrollView_ = MakeScrollBox(bk::ScrollAxis::Vertical, 180.0F);
        auto verticalContent = std::make_shared<bk::VBox>();
        verticalContent->SetSpacing(10.0F);
        for (int index = 0; index < 16; ++index)
        {
            auto item = std::make_shared<bk::Button>("Vertical Item " + std::to_string(index + 1));
            item->SetCornerRadius(10.0F);
            item->SetBackgroundColor(index % 2 == 0
                ? bk::ColorRGBA(39, 48, 63, 255)
                : bk::ColorRGBA(33, 40, 54, 255));
            item->SetPressedBackgroundColor(bk::ColorRGBA(27, 110, 215, 255));
            item->SetPadding(10.0F, 14.0F);
            item->OnClick().Connect([this, index](bk::Button&) {
                AppendLog("clicked vertical item " + std::to_string(index + 1));
                RefreshStateTexts();
            });
            listButtons_.push_back(item);
            verticalContent->AddChild(item);
        }
        verticalScrollView_->SetContent(verticalContent);

        horizontalScrollView_ = MakeScrollBox(bk::ScrollAxis::Horizontal, 132.0F);
        auto horizontalContent = std::make_shared<bk::HBox>();
        horizontalContent->SetSpacing(12.0F);
        for (int index = 0; index < 12; ++index)
        {
            auto chip = std::make_shared<bk::Button>("Card " + std::to_string(index + 1));
            chip->SetWidth(150.0F);
            chip->SetHeight(92.0F);
            chip->SetCornerRadius(12.0F);
            chip->SetBackgroundColor(bk::ColorRGBA(
                54 + (index % 4) * 24,
                72 + (index % 3) * 18,
                110 + (index % 5) * 16,
                255));
            chip->SetPressedBackgroundColor(bk::ColorRGBA(27, 110, 215, 255));
            chip->OnClick().Connect([this, index](bk::Button&) {
                AppendLog("clicked horizontal card " + std::to_string(index + 1));
                RefreshStateTexts();
            });
            horizontalCards_.push_back(chip);
            horizontalContent->AddChild(chip);
        }
        horizontalScrollView_->SetContent(horizontalContent);

        canvasScrollView_ = MakeScrollBox(bk::ScrollAxis::Both, 190.0F);
        auto canvas = std::make_shared<bk::View>();
        canvas->SetWidth(880.0F);
        canvas->SetHeight(540.0F);
        canvas->SetDrawBackground(true);
        canvas->SetBackgroundColor(bk::ColorRGBA(24, 30, 40, 255));
        canvas->SetCornerRadius(10.0F);

        auto canvasLayer = std::make_shared<bk::VBox>();
        canvasLayer->SetSpacing(16.0F);
        canvasLayer->SetPadding(16.0F);
        canvasLayer->SetWidth(880.0F);
        canvasLayer->SetHeight(540.0F);
        canvasLayer->SetDrawBackground(true);
        canvasLayer->SetBackgroundColor(bk::ColorRGBA(24, 30, 40, 255));
        canvasLayer->SetCornerRadius(10.0F);

        for (int row = 0; row < 6; ++row)
        {
            auto line = std::make_shared<bk::HBox>();
            line->SetSpacing(14.0F);
            for (int column = 0; column < 5; ++column)
            {
                const int tileIndex = row * 5 + column + 1;
                auto tile = std::make_shared<bk::Button>("Tile " + std::to_string(tileIndex));
                tile->SetWidth(150.0F);
                tile->SetHeight(70.0F);
                tile->SetCornerRadius(10.0F);
                tile->SetBackgroundColor(bk::ColorRGBA(
                    36 + row * 10,
                    54 + column * 12,
                    92 + ((row + column) % 4) * 24,
                    255));
                tile->SetPressedBackgroundColor(bk::ColorRGBA(27, 110, 215, 255));
                tile->OnClick().Connect([this, tileIndex](bk::Button&) {
                    AppendLog("clicked canvas tile " + std::to_string(tileIndex));
                    RefreshStateTexts();
                });
                canvasTiles_.push_back(tile);
                line->AddChild(tile);
            }
            canvasLayer->AddChild(line);
        }
        canvasScrollView_->SetContent(canvasLayer);

        sections->AddChild(MakeScrollSection("Vertical", verticalScrollView_));
        sections->AddChild(MakeScrollSection("Horizontal", horizontalScrollView_));
        sections->AddChild(MakeScrollSection("Both", canvasScrollView_));

        card->AddChild(hint);
        card->AddChild(sections);
        return card;
    }

    std::shared_ptr<bk::View> BuildStateCard()
    {
        auto card = MakeCard("State");
        stateTitle_ = std::make_shared<bk::Label>("No interaction yet");
        stateTitle_->SetFontSize(20.0F);
        stateTitle_->SetTextColor(bk::ColorRGBA(244, 247, 255, 255));

        stateDetails_ = std::make_shared<bk::Label>();
        stateDetails_->SetFontSize(15.0F);
        stateDetails_->SetTextColor(bk::ColorRGBA(180, 190, 208, 255));

        card->AddChild(stateTitle_);
        card->AddChild(stateDetails_);
        return card;
    }

    std::shared_ptr<bk::View> BuildPreviewCard()
    {
        auto card = MakeCard("Preview");
        previewPanel_ = std::make_shared<bk::View>();
        previewPanel_->SetHeight(180.0F);
        previewPanel_->SetDrawBackground(true);
        previewPanel_->SetBackgroundColor(bk::ColorRGBA(34, 94, 162, 255));
        previewPanel_->SetCornerRadius(16.0F);
        previewPanel_->SetShadowEnabled(true);
        previewPanel_->SetShadowColor(bk::ColorRGBA(0, 0, 0, 92));
        previewPanel_->SetShadowBlurRadius(26.0F);
        previewPanel_->SetShadowOffset(0.0F, 10.0F);

        previewLabel_ = std::make_shared<bk::Label>();
        previewLabel_->SetFontSize(15.0F);
        previewLabel_->SetTextColor(bk::ColorRGBA(185, 194, 211, 255));

        auto helper = std::make_shared<bk::Label>("Slider 会驱动预览面板缩放与进度条，CheckBox 会影响按钮与布局。");
        helper->SetFontSize(14.0F);
        helper->SetTextColor(bk::ColorRGBA(150, 160, 178, 255));

        card->AddChild(previewPanel_);
        card->AddChild(previewLabel_);
        card->AddChild(helper);
        return card;
    }

    std::shared_ptr<bk::VBox> MakeCard(const std::string& titleText)
    {
        auto card = std::make_shared<bk::VBox>();
        card->SetSpacing(12.0F);
        card->SetPadding(16.0F);
        card->SetDrawBackground(true);
        card->SetBackgroundColor(bk::ColorRGBA(28, 34, 45, 255));
        card->SetCornerRadius(12.0F);

        auto title = std::make_shared<bk::Label>(titleText);
        title->SetFontSize(18.0F);
        title->SetTextColor(bk::ColorRGBA(236, 241, 252, 255));
        card->AddChild(title);
        return card;
    }

    void WireEvents()
    {
        primaryButton_->OnClick().Connect([this](bk::Button&) {
            AppendLog("primary button clicked");
            stateTitle_->SetText("Primary action fired");
        });
        primaryButton_->OnPressed().Connect([this](bk::Button&) {
            AppendLog("primary button pressed");
        });
        primaryButton_->OnReleased().Connect([this](bk::Button&) {
            AppendLog("primary button released");
        });

        secondaryButton_->OnClick().Connect([this](bk::Button&) {
            const bool nextEnabled = !soundCheckBox_->IsEnabled();
            soundCheckBox_->SetEnabled(nextEnabled);
            compactCheckBox_->SetEnabled(nextEnabled);
            intensitySlider_->SetEnabled(nextEnabled);
            AppendLog(std::string("toggled input enabled to ") + (nextEnabled ? "true" : "false"));
            RefreshStateTexts();
        });

        soundCheckBox_->OnValueChanged().Connect([this](bk::CheckBox&, bool checked) {
            AppendLog(std::string("sound checkbox = ") + (checked ? "true" : "false"));
            primaryButton_->SetBackgroundColor(checked
                ? bk::ColorRGBA(64, 163, 255, 255)
                : bk::ColorRGBA(90, 98, 112, 255));
            RefreshStateTexts();
        });

        compactCheckBox_->OnValueChanged().Connect([this](bk::CheckBox&, bool checked) {
            AppendLog(std::string("compact mode = ") + (checked ? "true" : "false"));
            if (checked)
            {
                verticalScrollView_->SetHeight(150.0F);
                horizontalScrollView_->SetHeight(112.0F);
                canvasScrollView_->SetHeight(160.0F);
                previewPanel_->SetCornerRadius(10.0F);
            }
            else
            {
                verticalScrollView_->SetHeight(180.0F);
                horizontalScrollView_->SetHeight(132.0F);
                canvasScrollView_->SetHeight(190.0F);
                previewPanel_->SetCornerRadius(16.0F);
            }
            InvalidateLayout();
            RefreshStateTexts();
        });

        intensitySlider_->OnSlidingStarted().Connect([this](bk::Slider&) {
            AppendLog("slider drag started");
        });
        intensitySlider_->OnSlidingEnded().Connect([this](bk::Slider&) {
            AppendLog("slider drag ended");
        });
        intensitySlider_->OnValueChanged().Connect([this](bk::Slider&, float value) {
            AppendLog("slider value = " + FormatPercent(value));
            progressBar_->SetValue(value);
            previewPanel_->SetScale(0.92F + value * 0.22F);
            if (verticalScrollView_)
            {
                verticalScrollView_->SetInertiaDecay(4.0F + value * 10.0F);
                horizontalScrollView_->SetInertiaDecay(4.0F + value * 10.0F);
                canvasScrollView_->SetInertiaDecay(4.0F + value * 10.0F);
            }
            previewPanel_->SetBackgroundColor(bk::ColorRGBA(
                static_cast<int>(42 + value * 76.0F),
                static_cast<int>(90 + value * 80.0F),
                static_cast<int>(150 + value * 70.0F),
                255));
            RefreshStateTexts();
        });
    }

    void AppendLog(const std::string& message)
    {
        logLines_.push_back(message);
        if (logLines_.size() > 6)
        {
            logLines_.erase(logLines_.begin());
        }
        bklog.info("[controls_demo] " + message);
    }

    void RefreshStateTexts()
    {
        intensityLabel_->SetText("Intensity: " + FormatPercent(intensitySlider_->GetValue()));

        std::ostringstream state;
        state << "sound=" << (soundCheckBox_->IsChecked() ? "on" : "off")
              << "  compact=" << (compactCheckBox_->IsChecked() ? "on" : "off")
              << "  slider=" << FormatPercent(intensitySlider_->GetValue());
        stateTitle_->SetText(state.str());

        std::ostringstream details;
        details << "enabled=" << (soundCheckBox_->IsEnabled() ? "true" : "false")
                << "\nprogress=" << FormatPercent(progressBar_->GetValue())
                << "\nverticalOffset=" << (verticalScrollView_ ? FormatNumber(verticalScrollView_->GetScrollY()) : "0.00")
                << "\nhorizontalOffset=" << (horizontalScrollView_ ? FormatNumber(horizontalScrollView_->GetScrollX()) : "0.00")
                << "\ncanvasOffset=" << (canvasScrollView_
                    ? (FormatNumber(canvasScrollView_->GetScrollX(), 1) + "," + FormatNumber(canvasScrollView_->GetScrollY(), 1))
                    : "0.0,0.0")
                << "\nrecent:";
        for (const std::string& line : logLines_)
        {
            details << "\n- " << line;
        }
        stateDetails_->SetText(details.str());

        previewLabel_->SetText(
            "Preview scale " + FormatPercent((previewPanel_->GetScale() - 0.92F) / 0.22F));
    }

    std::shared_ptr<bk::ScrollBox> MakeScrollBox(bk::ScrollAxis axis, float height) const
    {
        auto scrollBox = std::make_shared<bk::ScrollBox>(axis);
        scrollBox->SetHeight(height);
        scrollBox->SetDrawBackground(true);
        scrollBox->SetBackgroundColor(bk::ColorRGBA(19, 24, 32, 255));
        scrollBox->SetCornerRadius(12.0F);
        scrollBox->SetPadding(10.0F);
        scrollBox->SetScrollBarThickness(9.0F);
        scrollBox->SetScrollBarTrackColor(bk::ColorRGBA(12, 15, 20, 140));
        scrollBox->SetScrollBarThumbColor(bk::ColorRGBA(168, 180, 201, 224));
        scrollBox->SetInertiaEnabled(true);
        scrollBox->SetInertiaDecay(7.0F);
        return scrollBox;
    }

    std::shared_ptr<bk::View> MakeScrollSection(
        const std::string& titleText,
        const std::shared_ptr<bk::ScrollBox>& scrollBox) const
    {
        auto section = std::make_shared<bk::VBox>();
        section->SetSpacing(8.0F);

        auto title = std::make_shared<bk::Label>(titleText);
        title->SetFontSize(15.0F);
        title->SetTextColor(bk::ColorRGBA(196, 206, 222, 255));

        section->AddChild(title);
        section->AddChild(scrollBox);
        return section;
    }

    std::shared_ptr<bk::Button> primaryButton_{};
    std::shared_ptr<bk::Button> secondaryButton_{};
    std::shared_ptr<bk::Button> disabledButton_{};
    std::shared_ptr<bk::CheckBox> soundCheckBox_{};
    std::shared_ptr<bk::CheckBox> compactCheckBox_{};
    std::shared_ptr<bk::Slider> intensitySlider_{};
    std::shared_ptr<bk::ProgressBar> progressBar_{};
    std::shared_ptr<bk::ScrollBox> verticalScrollView_{};
    std::shared_ptr<bk::ScrollBox> horizontalScrollView_{};
    std::shared_ptr<bk::ScrollBox> canvasScrollView_{};
    std::vector<std::shared_ptr<bk::Button>> listButtons_{};
    std::vector<std::shared_ptr<bk::Button>> horizontalCards_{};
    std::vector<std::shared_ptr<bk::Button>> canvasTiles_{};
    std::shared_ptr<bk::View> previewPanel_{};
    std::shared_ptr<bk::Label> stateTitle_{};
    std::shared_ptr<bk::Label> stateDetails_{};
    std::shared_ptr<bk::Label> intensityLabel_{};
    std::shared_ptr<bk::Label> previewLabel_{};
    std::shared_ptr<bk::ScrollBox> pageScrollBox_{};
    std::shared_ptr<bk::VBox> pageContent_{};
    std::vector<std::string> logLines_{};
};
}

int main(int argc, char** argv)
{
    try
    {
        bk::ApplicationHost host;
        bk::ApplicationHostDesc desc;
        desc.window.title = "BeikUI Controls Demo";
        desc.window.width = static_cast<int>(kDesignWidth);
        desc.window.height = static_cast<int>(kDesignHeight);
        desc.logicalSize = bk::Vector2{kDesignWidth, kDesignHeight};
        desc.clearColor = bk::ColorRGBA(14, 18, 24, 255);
        desc.application.name = "BeikUI Controls Demo";
        desc.application.version = "0.2.0";
        desc.application.identifier = "bkui.demo.controls";
        desc.application.logger.level = bk::LogLevel::Info;
        desc.application.logger.enableConsole = true;
        desc.application.logger.enableColor =
#if defined(BKUI_PLATFORM_SWITCH)
            false;
#else
            true;
#endif

        if (!host.Initialize(desc, argc, const_cast<const char* const*>(argv)))
        {
            std::fprintf(stderr, "Failed to initialize ApplicationHost.\n");
            return 1;
        }

        auto page = std::make_shared<ControlsShowcasePage>();
        host.GetApplication().AddView(page);
        host.GetApplication().SetLogOverlayVisible(true);
        bklog.info("Controls demo initialized.");

        const std::uint64_t executedFrames = host.MainLoop(bk::MainLoopDesc{
            1.0F / 60.0F,
            0,
            true,
            true,
            true,
        });
        bklog.info("Controls demo exited after " + std::to_string(executedFrames) + " frames.");
        host.Shutdown();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Fatal error: %s\n", ex.what());
        return 1;
    }

    return 0;
}
