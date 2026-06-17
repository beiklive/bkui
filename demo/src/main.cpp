#include <bkui/bkui.hpp>

#include "demo_support.cpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


namespace
{
using bk::operator""_i18n;
using demo::EnsureWindowSize;
constexpr float kDesignWidth = 1280.0F;
constexpr float kDesignHeight = 720.0F;

std::string DisplayName(const std::shared_ptr<bk::View>& view)
{
    if (!view)
    {
        return "None";
    }

    return view->GetName().empty() ? "Unnamed" : view->GetName();
}

std::string DisplayName(const bk::View* view)
{
    if (view == nullptr)
    {
        return "None";
    }

    return view->GetName().empty() ? "Unnamed" : view->GetName();
}

void SetDebugWireframeRecursive(const std::shared_ptr<bk::View>& view, bool enabled)
{
    if (!view)
    {
        return;
    }

    view->SetDebugWireframeEnabled(enabled);
    for (const auto& child : view->Children())
    {
        SetDebugWireframeRecursive(child, enabled);
    }
}

class ActionButton : public bk::Button
{
public:
    using Callback = std::function<void(ActionButton&)>;

    ActionButton(std::string title, std::string subtitle)
        : bk::Button(std::move(title))
        , subtitle_(std::move(subtitle))
    {
        SetPadding(14.0F);
        SetMinHeight(92.0F);
        SetCornerRadius(16.0F);
        SetShadowEnabled(true);
        SetShadowOffset(0.0F, 8.0F);
        SetShadowBlurRadius(14.0F);
        SetShadowSpread(2.0F);
        SetShadowColor(bk::Color{0.0F, 0.0F, 0.0F, 0.18F});
    }

    void SetSubtitle(std::string subtitle)
    {
        subtitle_ = std::move(subtitle);
        InvalidateLayout();
    }

    void SetCallback(Callback callback)
    {
        callback_ = std::move(callback);
    }

    void Draw(bk::RenderQueue& queue) const override
    {
        bk::Box::Draw(queue);

        const bk::Rect content = GetContentFrame();
        queue.PushText(
            bk::Rect{content.x, content.y, content.width, 28.0F},
            GetText(),
            22.0F,
            GetTextColor());
        queue.PushText(
            bk::Rect{content.x, content.y + 34.0F, content.width, std::max(0.0F, content.height - 34.0F)},
            subtitle_,
            15.0F,
            bk::Color{0.86F, 0.90F, 0.96F, 1.0F});
    }

protected:
    void Click(const bk::Vector2&) override
    {
        if (callback_)
        {
            callback_(*this);
        }
    }

private:
    std::string subtitle_;
    Callback callback_{};
};

class DemoModalView final : public bk::View
{
public:
    DemoModalView()
    {
        SetName("ModalOverlay");
        SetZIndex(100);

        dialog_ = std::make_shared<bk::VBox>();
        dialog_->SetName("ModalDialog");
        dialog_->SetDrawBackground(true);
        dialog_->SetBackgroundColor(bk::Color{0.12F, 0.15F, 0.23F, 1.0F});
        dialog_->SetCornerRadius(24.0F);
        dialog_->SetShadowEnabled(true);
        dialog_->SetShadowOffset(0.0F, 18.0F);
        dialog_->SetShadowBlurRadius(24.0F);
        dialog_->SetShadowSpread(4.0F);
        dialog_->SetShadowColor(bk::Color{0.0F, 0.0F, 0.0F, 0.26F});
        dialog_->SetPaddingTop(22.0F);
        dialog_->SetPaddingRight(24.0F);
        dialog_->SetPaddingBottom(22.0F);
        dialog_->SetPaddingLeft(24.0F);
        dialog_->SetSpacing(14.0F);

        title_ = MakeLabel("modal/title"_i18n.str(), 30.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        subtitle_ = MakeLabel("modal/subtitle"_i18n.str(), 17.0F, bk::Color{0.77F, 0.84F, 0.93F, 1.0F});
        modalFocusLabel_ = MakeLabel("", 16.0F, bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        modalDefaultLabel_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        modalLastLabel_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        modalActionLabel_ = MakeLabel("modal/action_ready"_i18n.str(), 16.0F, bk::Color{0.98F, 0.85F, 0.50F, 1.0F});

        topRow_ = std::make_shared<bk::HBox>();
        topRow_->SetName("ModalTopRow");
        topRow_->SetSpacing(12.0F);
        topRow_->SetMarginBottom(4.0F);
        bottomRow_ = std::make_shared<bk::HBox>();
        bottomRow_->SetName("ModalBottomRow");
        bottomRow_->SetSpacing(12.0F);

        primaryButton_ = MakeButton(
            "modal/confirm"_i18n.str(),
            "modal/confirm_hint"_i18n.str(),
            bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        secondaryButton_ = MakeButton(
            "modal/go_default"_i18n.str(),
            "modal/go_default_hint"_i18n.str(),
            bk::Color{0.18F, 0.67F, 0.53F, 1.0F});
        lastFocusButton_ = MakeButton(
            "modal/go_last"_i18n.str(),
            "modal/go_last_hint"_i18n.str(),
            bk::Color{0.77F, 0.48F, 0.24F, 1.0F});
        closeButton_ = MakeButton(
            "modal/close"_i18n.str(),
            "modal/close_hint"_i18n.str(),
            bk::Color{0.78F, 0.32F, 0.36F, 1.0F});

        primaryButton_->SetCallback([this](ActionButton&) {
            SetAction("Confirm clicked");
        });
        secondaryButton_->SetCallback([this](ActionButton&) {
            SetAction("Request default focus");
            RequestDefaultFocus();
        });
        lastFocusButton_->SetCallback([this](ActionButton&) {
            SetAction("Request last focus");
            RequestLastFocus();
        });
        closeButton_->SetCallback([this](ActionButton&) {
            SetAction("Close requested");
            closeRequested_ = true;
        });

        topRow_->AddChild(primaryButton_);
        topRow_->AddChild(secondaryButton_);
        bottomRow_->AddChild(lastFocusButton_);
        bottomRow_->AddChild(closeButton_);

        dialog_->AddChild(title_);
        dialog_->AddChild(subtitle_);
        dialog_->AddChild(modalFocusLabel_);
        dialog_->AddChild(modalDefaultLabel_);
        dialog_->AddChild(modalLastLabel_);
        dialog_->AddChild(modalActionLabel_);
        dialog_->AddChild(topRow_);
        dialog_->AddChild(bottomRow_);
        AddChild(dialog_);

        SetDefaultFocusView(primaryButton_);
        dialog_->SetDefaultFocusView(primaryButton_);

        primaryButton_->SetNavigationTarget(bk::NavigationDirection::Right, secondaryButton_);
        primaryButton_->SetNavigationTarget(bk::NavigationDirection::Down, lastFocusButton_);
        secondaryButton_->SetNavigationTarget(bk::NavigationDirection::Left, primaryButton_);
        secondaryButton_->SetNavigationTarget(bk::NavigationDirection::Down, closeButton_);
        lastFocusButton_->SetNavigationTarget(bk::NavigationDirection::Up, primaryButton_);
        lastFocusButton_->SetNavigationTarget(bk::NavigationDirection::Right, closeButton_);
        closeButton_->SetNavigationTarget(bk::NavigationDirection::Up, secondaryButton_);
        closeButton_->SetNavigationTarget(bk::NavigationDirection::Left, lastFocusButton_);
    }

    bool ConsumeCloseRequested()
    {
        const bool requested = closeRequested_;
        closeRequested_ = false;
        return requested;
    }

    void ApplyWireframe(bool enabled)
    {
        SetDebugWireframeRecursive(shared_from_this(), enabled);
    }

    void SyncStatus(const bk::Application& app)
    {
        modalFocusLabel_->SetText("modal/focused"_i18n(DisplayName(app.GetFocusedView())).str());
        modalDefaultLabel_->SetText("modal/default_val"_i18n(DisplayName(GetDefaultFocusView())).str());
        modalLastLabel_->SetText("modal/last"_i18n(DisplayName(GetLastFocusedView())).str());
    }

    void Layout() override
    {
        const float dialogWidth = 720.0F;
        const float dialogHeight = 332.0F;
        dialog_->SetFrame(bk::Rect{
            frame_.x + (frame_.width - dialogWidth) * 0.5F,
            frame_.y + (frame_.height - dialogHeight) * 0.5F,
            dialogWidth,
            dialogHeight});
        dialog_->Layout();
        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        queue.PushRect(frame_, bk::Color{0.03F, 0.04F, 0.08F, 0.72F});
    }

    void Update(float) override
    {
        if (bk::Application* app = bk::Application::Active())
        {
            SyncStatus(*app);
        }
    }

    void Click(const bk::Vector2& position) override
    {
        if (!dialog_->ContainsPoint(position))
        {
            closeRequested_ = true;
        }
    }

private:
    static std::shared_ptr<bk::Label> MakeLabel(const std::string& text, float fontSize, const bk::Color& color)
    {
        auto label = std::make_shared<bk::Label>(text);
        label->SetFontSize(fontSize);
        label->SetTextColor(color);
        return label;
    }

    static std::shared_ptr<ActionButton> MakeButton(const std::string& title, const std::string& subtitle, const bk::Color& color)
    {
        auto button = std::make_shared<ActionButton>(title, subtitle);
        button->SetBackgroundColor(color);
        button->SetFlexGrow(1.0F);
        button->SetTextColor(bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
        return button;
    }

    void SetAction(std::string action)
    {
        modalActionLabel_->SetText("modal/action"_i18n(std::move(action)).str());
    }

    std::shared_ptr<bk::VBox> dialog_;
    std::shared_ptr<bk::Label> title_;
    std::shared_ptr<bk::Label> subtitle_;
    std::shared_ptr<bk::Label> modalFocusLabel_;
    std::shared_ptr<bk::Label> modalDefaultLabel_;
    std::shared_ptr<bk::Label> modalLastLabel_;
    std::shared_ptr<bk::Label> modalActionLabel_;
    std::shared_ptr<bk::HBox> topRow_;
    std::shared_ptr<bk::HBox> bottomRow_;
    std::shared_ptr<ActionButton> primaryButton_;
    std::shared_ptr<ActionButton> secondaryButton_;
    std::shared_ptr<ActionButton> lastFocusButton_;
    std::shared_ptr<ActionButton> closeButton_;
    bool closeRequested_ = false;
};

class DemoPage final : public bk::View
{
public:
    DemoPage()
    {
        SetName("RootPage");

        rootScroll_ = std::make_shared<bk::ScrollView>(bk::ScrollAxis::Vertical);
        rootScroll_->SetName("RootScroll");

        rootColumn_ = std::make_shared<bk::VBox>();
        rootColumn_->SetName("RootColumn");
        rootColumn_->SetPaddingTop(24.0F);
        rootColumn_->SetPaddingRight(24.0F);
        rootColumn_->SetPaddingBottom(24.0F);
        rootColumn_->SetPaddingLeft(24.0F);
        rootColumn_->SetSpacing(20.0F);
        rootColumn_->SetWidth(kDesignWidth);
        rootColumn_->SetMinHeight(kDesignHeight);

        headerBar_ = std::make_shared<bk::HBox>();
        headerBar_->SetName("HeaderBar");
        headerBar_->SetDrawBackground(true);
        headerBar_->SetBackgroundColor(bk::Color{0.11F, 0.16F, 0.25F, 1.0F});
        headerBar_->SetPaddingTop(18.0F);
        headerBar_->SetPaddingRight(22.0F);
        headerBar_->SetPaddingBottom(18.0F);
        headerBar_->SetPaddingLeft(22.0F);
        headerBar_->SetMinHeight(92.0F);

        headerText_ = std::make_shared<bk::VBox>();
        headerText_->SetName("HeaderText");
        headerText_->SetSpacing(6.0F);
        headerText_->SetFlexGrow(1.0F);
        headerTitle_ = MakeLabel("page/title"_i18n.str(), 33.0F, bk::Color{0.98F, 0.99F, 1.0F, 1.0F});
        headerSubtitle_ = MakeLabel("page/subtitle"_i18n.str(), 17.0F, bk::Color{0.77F, 0.84F, 0.93F, 1.0F});
        headerText_->AddChild(headerTitle_);
        headerText_->AddChild(headerSubtitle_);
        headerHint_ = MakeLabel("page/hint"_i18n.str(), 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
        headerHint_->SetWidth(360.0F);
        headerBar_->AddChild(headerText_);
        headerBar_->AddChild(headerHint_);

        bodyRow_ = std::make_shared<bk::HBox>();
        bodyRow_->SetName("BodyRow");
        bodyRow_->SetSpacing(20.0F);
        bodyRow_->SetFlexGrow(1.0F);

        leftColumn_ = std::make_shared<bk::VBox>();
        leftColumn_->SetName("LeftColumn");
        leftColumn_->SetWidth(320.0F);
        leftColumn_->SetSpacing(18.0F);

        navPanel_ = MakePanel("NavPanel", bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        navTitle_ = MakeLabel("nav/title"_i18n.str(), 24.0F, bk::Color{0.10F, 0.14F, 0.21F, 1.0F});
        navTip_ = MakeLabel("nav/tip"_i18n.str(), 16.0F, bk::Color{0.31F, 0.38F, 0.49F, 1.0F});
        navHome_ = MakeActionButton("nav/home"_i18n.str(), "nav/home_hint"_i18n.str(), bk::Color{0.22F, 0.49F, 0.93F, 1.0F});
        navGallery_ = MakeActionButton("nav/gallery"_i18n.str(), "nav/gallery_hint"_i18n.str(), bk::Color{0.20F, 0.64F, 0.55F, 1.0F});
        openModalButton_ = MakeActionButton("nav/open_modal"_i18n.str(), "nav/open_modal_hint"_i18n.str(), bk::Color{0.79F, 0.45F, 0.26F, 1.0F});
        openModalButton_->SetMarginTop(4.0F);
        navPanel_->AddChild(navTitle_);
        navPanel_->AddChild(navTip_);
        navPanel_->AddChild(navHome_);
        navPanel_->AddChild(navGallery_);
        navPanel_->AddChild(openModalButton_);

        focusPanel_ = MakePanel("FocusPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        focusTitle_ = MakeLabel("focus/title"_i18n.str(), 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        focusTip_ = MakeLabel("focus/tip"_i18n.str(), 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        focusDefaultButton_ = MakeActionButton("focus/go_default"_i18n.str(), "focus/go_default_hint"_i18n.str(), bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        focusLastButton_ = MakeActionButton("focus/go_last"_i18n.str(), "focus/go_last_hint"_i18n.str(), bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        focusClearButton_ = MakeActionButton("focus/clear"_i18n.str(), "focus/clear_hint"_i18n.str(), bk::Color{0.72F, 0.34F, 0.39F, 1.0F});
        focusWireframeButton_ = MakeActionButton("focus/wireframe_off"_i18n.str(), "focus/wireframe_hint"_i18n.str(), bk::Color{0.43F, 0.40F, 0.86F, 1.0F});
        focusMotionButton_ = MakeActionButton("focus/motion_on"_i18n.str(), "focus/motion_hint"_i18n.str(), bk::Color{0.86F, 0.36F, 0.72F, 1.0F});
        focusLayerTrailButton_ = MakeActionButton("focus/trail_on"_i18n.str(), "focus/trail_hint"_i18n.str(), bk::Color{0.34F, 0.62F, 0.88F, 1.0F});
        focusThemeLabel_ = MakeLabel("focus/theme_label"_i18n("Aurora").str(), 15.0F, bk::Color{0.84F, 0.89F, 0.97F, 1.0F});
        focusThemeAuroraButton_ = MakeActionButton("focus/theme_aurora"_i18n.str(), "focus/theme_aurora_hint"_i18n.str(), bk::Color{0.24F, 0.52F, 0.95F, 1.0F});
        focusThemeSunsetButton_ = MakeActionButton("focus/theme_sunset"_i18n.str(), "focus/theme_sunset_hint"_i18n.str(), bk::Color{0.92F, 0.46F, 0.34F, 1.0F});
        focusThemeMintButton_ = MakeActionButton("focus/theme_mint"_i18n.str(), "focus/theme_mint_hint"_i18n.str(), bk::Color{0.22F, 0.72F, 0.64F, 1.0F});
        focusPanel_->AddChild(focusTitle_);
        focusPanel_->AddChild(focusTip_);
        focusPanel_->AddChild(focusDefaultButton_);
        focusPanel_->AddChild(focusLastButton_);
        focusPanel_->AddChild(focusClearButton_);
        focusPanel_->AddChild(focusWireframeButton_);
        focusPanel_->AddChild(focusMotionButton_);
        focusPanel_->AddChild(focusLayerTrailButton_);
        focusPanel_->AddChild(focusThemeLabel_);
        focusPanel_->AddChild(focusThemeAuroraButton_);
        focusPanel_->AddChild(focusThemeSunsetButton_);
        focusPanel_->AddChild(focusThemeMintButton_);

        langPanel_ = MakePanel("LangPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        langTitle_ = MakeLabel("lang/title"_i18n.str(), 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        langHint_ = MakeLabel("lang/hint"_i18n.str(), 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        langZhButton_ = MakeActionButton("lang/zh"_i18n.str(), "lang/zh_hint"_i18n.str(), bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        langEnButton_ = MakeActionButton("lang/en"_i18n.str(), "lang/en_hint"_i18n.str(), bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        langPanel_->AddChild(langTitle_);
        langPanel_->AddChild(langHint_);
        langPanel_->AddChild(langZhButton_);
        langPanel_->AddChild(langEnButton_);

        leftColumn_->AddChild(navPanel_);
        leftColumn_->AddChild(focusPanel_);
        leftColumn_->AddChild(langPanel_);

        rightColumn_ = std::make_shared<bk::VBox>();
        rightColumn_->SetName("RightColumn");
        rightColumn_->SetFlexGrow(1.0F);
        rightColumn_->SetSpacing(18.0F);

        statusPanel_ = MakePanel("StatusPanel", bk::Color{0.11F, 0.15F, 0.23F, 1.0F});
        statusTitle_ = MakeLabel("status/title"_i18n.str(), 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        statusFocus_ = MakeLabel("", 16.0F, bk::Color{0.94F, 0.96F, 1.0F, 1.0F});
        statusDefault_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusLast_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusPointer_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusKey_ = MakeLabel("status/key_idle"_i18n.str(), 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
        statusAction_ = MakeLabel("status/action_ready"_i18n.str(), 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
        statusModal_ = MakeLabel("Top Views: 1", 16.0F, bk::Color{0.76F, 0.83F, 0.92F, 1.0F});
        statusPanel_->AddChild(statusTitle_);
        statusPanel_->AddChild(statusFocus_);
        statusPanel_->AddChild(statusDefault_);
        statusPanel_->AddChild(statusLast_);
        statusPanel_->AddChild(statusPointer_);
        statusPanel_->AddChild(statusKey_);
        statusPanel_->AddChild(statusAction_);
        statusPanel_->AddChild(statusModal_);

        lowerRow_ = std::make_shared<bk::HBox>();
        lowerRow_->SetName("LowerRow");
        lowerRow_->SetSpacing(18.0F);
        lowerRow_->SetFlexGrow(1.0F);

        treePanel_ = MakePanel("TreePanel", bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        treePanel_->SetWidth(338.0F);
        treeTitle_ = MakeLabel("tree/title"_i18n.str(), 23.0F, bk::Color{0.10F, 0.14F, 0.21F, 1.0F});
        treeSubtitle_ = MakeLabel("tree/subtitle"_i18n.str(), 16.0F, bk::Color{0.31F, 0.38F, 0.49F, 1.0F});
        treePanel_->AddChild(treeTitle_);
        treePanel_->AddChild(treeSubtitle_);
        for (int index = 0; index < 16; ++index)
        {
            auto line = MakeLabel("", 15.0F, bk::Color{0.16F, 0.22F, 0.32F, 1.0F});
            line->SetMarginLeft(4.0F);
            treeLines_.push_back(line);
            treePanel_->AddChild(line);
        }

        actionPanel_ = MakePanel("ActionPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        actionPanel_->SetFlexGrow(1.0F);
        actionTitle_ = MakeLabel("action/title"_i18n.str(), 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        actionSubtitle_ = MakeLabel("action/subtitle"_i18n.str(), 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        actionRowTop_ = std::make_shared<bk::HBox>();
        actionRowTop_->SetName("ActionRowTop");
        actionRowTop_->SetSpacing(12.0F);
        actionRowBottom_ = std::make_shared<bk::HBox>();
        actionRowBottom_->SetName("ActionRowBottom");
        actionRowBottom_->SetSpacing(12.0F);

        cardOverview_ = MakeActionButton("action/overview"_i18n.str(), "action/overview_hint"_i18n.str(), bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        cardMetrics_ = MakeActionButton("action/metrics"_i18n.str(), "action/metrics_hint"_i18n.str(), bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        cardOpenModal_ = MakeActionButton("action/open_modal"_i18n.str(), "action/open_modal_hint"_i18n.str(), bk::Color{0.79F, 0.45F, 0.26F, 1.0F});
        cardRestoreFocus_ = MakeActionButton("action/restore_focus"_i18n.str(), "action/restore_focus_hint"_i18n.str(), bk::Color{0.44F, 0.41F, 0.86F, 1.0F});

        actionRowTop_->AddChild(cardOverview_);
        actionRowTop_->AddChild(cardMetrics_);
        actionRowBottom_->AddChild(cardOpenModal_);
        actionRowBottom_->AddChild(cardRestoreFocus_);
        actionPanel_->AddChild(actionTitle_);
        actionPanel_->AddChild(actionSubtitle_);
        actionPanel_->AddChild(actionRowTop_);
        actionPanel_->AddChild(actionRowBottom_);

        lowerRow_->AddChild(treePanel_);
        lowerRow_->AddChild(actionPanel_);

        rightColumn_->AddChild(statusPanel_);
        rightColumn_->AddChild(lowerRow_);

        bodyRow_->AddChild(leftColumn_);
        bodyRow_->AddChild(rightColumn_);

        rootColumn_->AddChild(headerBar_);
        rootColumn_->AddChild(bodyRow_);
        rootScroll_->SetContent(rootColumn_);
        AddChild(rootScroll_);

        SetDefaultFocusView(navHome_);

        BindCallbacks();
        BuildNavigation();
    }

    void SyncStatus(const bk::Application& app, const bk::InputState& input, const bk::Vector2& windowSize)
    {
        const bk::Vector2 pointer = input.touchCount > 0 ? input.touchPoints[0].position : input.mousePosition;
        statusFocus_->SetText("status/focused"_i18n(DisplayName(app.GetFocusedView())).str());
        statusDefault_->SetText("status/default_val"_i18n(DisplayName(GetDefaultFocusView())).str());
        statusLast_->SetText("status/last"_i18n(DisplayName(GetLastFocusedView())).str());
        statusPointer_->SetText(
            "status/pointer"_i18n(
                demo::FormatFloat(pointer.x, 0),
                demo::FormatFloat(pointer.y, 0),
                std::to_string(static_cast<int>(windowSize.x)),
                std::to_string(static_cast<int>(windowSize.y))).str());
        statusModal_->SetText("status/top_views"_i18n(std::to_string(app.GetViews().size())).str());

        if (input.keyPressed)
        {
            statusKey_->SetText("status/key_pressed"_i18n(input.lastKeyEvent.name).str());
        }
        else if (input.keyReleased)
        {
            statusKey_->SetText("status/key_released"_i18n(input.lastKeyEvent.name).str());
        }
        else if (input.mouseLeftPressed)
        {
            statusKey_->SetText("status/key_mouse"_i18n.str());
        }
        else if (input.touchCount > 0 && input.touchPoints[0].pressed)
        {
            statusKey_->SetText("status/key_touch"_i18n.str());
        }

        std::vector<std::string> lines;
        lines.reserve(treeLines_.size());
        const auto appendTree = [&](const std::shared_ptr<bk::View>& node, int depth, const auto& self) -> void {
            if (!node || !node->IsVisible() || lines.size() >= treeLines_.size())
            {
                return;
            }

            std::string line(static_cast<std::size_t>(depth) * 2, ' ');
            line += DisplayName(node);
            if (node->HasFocus())
            {
                line += " [F]";
            }
            lines.push_back(std::move(line));

            for (const auto& child : node->Children())
            {
                if (lines.size() >= treeLines_.size())
                {
                    break;
                }

                self(child, depth + 1, self);
            }
        };

        for (const auto& rootView : app.GetViews())
        {
            appendTree(rootView, 0, appendTree);
            if (lines.size() >= treeLines_.size())
            {
                break;
            }
        }

        for (std::size_t index = 0; index < treeLines_.size(); ++index)
        {
            treeLines_[index]->SetText(index < lines.size() ? lines[index] : "");
        }
    }

    bool ConsumeOpenModalRequest()
    {
        const bool requested = openModalRequested_;
        openModalRequested_ = false;
        return requested;
    }

    bool IsWireframeEnabled() const
    {
        return wireframeEnabled_;
    }

    void SetWindowSize(const bk::Vector2& windowSize)
    {
        windowSize_ = windowSize;
    }

    void ApplyWireframe(bool enabled)
    {
        wireframeEnabled_ = enabled;
        SetDebugWireframeRecursive(shared_from_this(), enabled);
        focusWireframeButton_->SetText(enabled ? "focus/wireframe_on"_i18n.str() : "focus/wireframe_off"_i18n.str());
    }

    void Layout() override
    {
        rootScroll_->SetFrame(frame_);
        rootScroll_->Layout();
        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        queue.PushRect(frame_, bk::Color{0.05F, 0.07F, 0.11F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x, frame_.y, frame_.width, 8.0F},
            bk::Color{0.20F, 0.52F, 0.97F, 1.0F});
    }

    void Update(float) override
    {
        if (bk::Application* app = bk::Application::Active())
        {
            SyncStatus(*app, app->GetInputState(), windowSize_);
            SyncFocusMotionButton();
            SyncFocusLayerTrailButton();
            SyncFocusThemeLabel();
        }
    }

private:
    static std::shared_ptr<bk::Label> MakeLabel(const std::string& text, float fontSize, const bk::Color& color)
    {
        auto label = std::make_shared<bk::Label>(text);
        label->SetFontSize(fontSize);
        label->SetTextColor(color);
        return label;
    }

    static std::shared_ptr<bk::VBox> MakePanel(const std::string& name, const bk::Color& color)
    {
        auto panel = std::make_shared<bk::VBox>();
        panel->SetName(name);
        panel->SetDrawBackground(true);
        panel->SetBackgroundColor(color);
        panel->SetCornerRadius(22.0F);
        panel->SetShadowEnabled(true);
        panel->SetShadowOffset(0.0F, 10.0F);
        panel->SetShadowBlurRadius(16.0F);
        panel->SetShadowSpread(2.0F);
        panel->SetShadowColor(bk::Color{0.0F, 0.0F, 0.0F, 0.16F});
        panel->SetPaddingTop(18.0F);
        panel->SetPaddingRight(18.0F);
        panel->SetPaddingBottom(18.0F);
        panel->SetPaddingLeft(18.0F);
        panel->SetSpacing(12.0F);
        return panel;
    }

    static std::shared_ptr<ActionButton> MakeActionButton(const std::string& title, const std::string& subtitle, const bk::Color& color)
    {
        auto button = std::make_shared<ActionButton>(title, subtitle);
        button->SetBackgroundColor(color);
        button->SetTextColor(bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
        return button;
    }

    void SetAction(std::string action)
    {
        statusAction_->SetText("status/action"_i18n(std::move(action)).str());
    }

    void ApplyFocusTheme(const std::string& name, const bk::Color& color1, const bk::Color& color2)
    {
        currentFocusThemeName_ = name;
        if (bk::Application* app = bk::Application::Active())
        {
            app->SetFocusHighlightColor1(color1);
            app->SetFocusHighlightColor2(color2);
        }
        SyncFocusThemeLabel();
    }

    void SyncFocusMotionButton()
    {
        if (bk::Application* app = bk::Application::Active())
        {
            const bool enabled = app->IsFocusHighlightMotionEnabled();
            focusMotionButton_->SetText(enabled ? "focus/motion_on"_i18n.str() : "focus/motion_off"_i18n.str());
        }
    }

    void SyncFocusLayerTrailButton()
    {
        if (bk::Application* app = bk::Application::Active())
        {
            const bool enabled = app->IsPreservingInactiveFocusHighlights();
            focusLayerTrailButton_->SetText(enabled ? "focus/trail_on"_i18n.str() : "focus/trail_off"_i18n.str());
        }
    }

    void SyncFocusThemeLabel()
    {
        focusThemeLabel_->SetText("focus/theme_label"_i18n(currentFocusThemeName_).str());
    }

    void RefreshTexts()
    {
        headerTitle_->SetText("page/title"_i18n.str());
        headerSubtitle_->SetText("page/subtitle"_i18n.str());
        headerHint_->SetText("page/hint"_i18n.str());
        navTitle_->SetText("nav/title"_i18n.str());
        navTip_->SetText("nav/tip"_i18n.str());
        navHome_->SetText("nav/home"_i18n.str());
        navGallery_->SetText("nav/gallery"_i18n.str());
        openModalButton_->SetText("nav/open_modal"_i18n.str());
        focusTitle_->SetText("focus/title"_i18n.str());
        focusTip_->SetText("focus/tip"_i18n.str());
        focusDefaultButton_->SetText("focus/go_default"_i18n.str());
        focusLastButton_->SetText("focus/go_last"_i18n.str());
        focusClearButton_->SetText("focus/clear"_i18n.str());
        focusThemeAuroraButton_->SetText("focus/theme_aurora"_i18n.str());
        focusThemeSunsetButton_->SetText("focus/theme_sunset"_i18n.str());
        focusThemeMintButton_->SetText("focus/theme_mint"_i18n.str());
        SyncFocusThemeLabel();
        SyncFocusMotionButton();
        SyncFocusLayerTrailButton();
        ApplyWireframe(wireframeEnabled_);
        statusTitle_->SetText("status/title"_i18n.str());
        statusKey_->SetText("status/key_idle"_i18n.str());
        statusAction_->SetText("status/action_ready"_i18n.str());
        treeTitle_->SetText("tree/title"_i18n.str());
        treeSubtitle_->SetText("tree/subtitle"_i18n.str());
        actionTitle_->SetText("action/title"_i18n.str());
        actionSubtitle_->SetText("action/subtitle"_i18n.str());
        cardOverview_->SetText("action/overview"_i18n.str());
        cardMetrics_->SetText("action/metrics"_i18n.str());
        cardOpenModal_->SetText("action/open_modal"_i18n.str());
        cardRestoreFocus_->SetText("action/restore_focus"_i18n.str());
        langTitle_->SetText("lang/title"_i18n.str());
        langHint_->SetText("lang/hint"_i18n.str());
        langZhButton_->SetText("lang/zh"_i18n.str());
        langEnButton_->SetText("lang/en"_i18n.str());
    }

    void BindCallbacks()
    {
        navHome_->SetCallback([this](ActionButton&) {
            SetAction("Home clicked");
        });
        navGallery_->SetCallback([this](ActionButton&) {
            SetAction("Gallery clicked");
        });
        openModalButton_->SetCallback([this](ActionButton&) {
            SetAction("Open detail view");
            openModalRequested_ = true;
        });

        focusDefaultButton_->SetCallback([this](ActionButton&) {
            SetAction("Request default focus");
            RequestDefaultFocus();
        });
        focusLastButton_->SetCallback([this](ActionButton&) {
            SetAction("Request last focus");
            RequestLastFocus();
        });
        focusClearButton_->SetCallback([this](ActionButton&) {
            SetAction("Clear current focus");
            ClearCurrentFocus();
        });
        focusWireframeButton_->SetCallback([this](ActionButton&) {
            ApplyWireframe(!wireframeEnabled_);
            SetAction(wireframeEnabled_ ? "Wireframe enabled" : "Wireframe disabled");
        });
        focusMotionButton_->SetCallback([this](ActionButton&) {
            if (bk::Application* app = bk::Application::Active())
            {
                const bool enabled = !app->IsFocusHighlightMotionEnabled();
                app->SetFocusHighlightMotionEnabled(enabled);
                SyncFocusMotionButton();
                SetAction(enabled ? "Focus motion enabled" : "Focus motion disabled");
            }
        });
        focusLayerTrailButton_->SetCallback([this](ActionButton&) {
            if (bk::Application* app = bk::Application::Active())
            {
                const bool enabled = !app->IsPreservingInactiveFocusHighlights();
                app->SetPreserveInactiveFocusHighlights(enabled);
                SyncFocusLayerTrailButton();
                SetAction(enabled ? "Layer focus trail enabled" : "Layer focus trail disabled");
            }
        });
        focusThemeAuroraButton_->SetCallback([this](ActionButton&) {
            ApplyFocusTheme(
                "Aurora",
                bk::Color{0.34F, 0.76F, 1.0F, 1.0F},
                bk::Color{0.76F, 0.52F, 1.0F, 1.0F});
            SetAction("Focus theme switched to Aurora");
        });
        focusThemeSunsetButton_->SetCallback([this](ActionButton&) {
            ApplyFocusTheme(
                "Sunset",
                bk::Color{1.0F, 0.72F, 0.34F, 1.0F},
                bk::Color{1.0F, 0.36F, 0.56F, 1.0F});
            SetAction("Focus theme switched to Sunset");
        });
        focusThemeMintButton_->SetCallback([this](ActionButton&) {
            ApplyFocusTheme(
                "Mint",
                bk::Color{0.42F, 1.0F, 0.86F, 1.0F},
                bk::Color{0.20F, 0.66F, 1.0F, 1.0F});
            SetAction("Focus theme switched to Mint");
        });

        langZhButton_->SetCallback([this](ActionButton&) {
            bk::I18n::Instance().SetLanguage("zh-CN");
            RefreshTexts();
            SetAction("Language switched to Simplified Chinese");
        });
        langEnButton_->SetCallback([this](ActionButton&) {
            bk::I18n::Instance().SetLanguage("en");
            RefreshTexts();
            SetAction("Language switched to English");
        });

        cardOverview_->SetCallback([this](ActionButton&) {
            SetAction("Overview clicked");
        });
        cardMetrics_->SetCallback([this](ActionButton&) {
            SetAction("Metrics clicked");
        });
        cardOpenModal_->SetCallback([this](ActionButton&) {
            SetAction("Open detail view from content");
            openModalRequested_ = true;
        });
        cardRestoreFocus_->SetCallback([this](ActionButton&) {
            SetAction("Restore last focus");
            RequestLastFocus();
        });
    }

    void BuildNavigation()
    {
        navHome_->SetNavigationTarget(bk::NavigationDirection::Down, navGallery_);
        navGallery_->SetNavigationTarget(bk::NavigationDirection::Up, navHome_);
        navGallery_->SetNavigationTarget(bk::NavigationDirection::Down, openModalButton_);
        openModalButton_->SetNavigationTarget(bk::NavigationDirection::Up, navGallery_);
        openModalButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusDefaultButton_);
        openModalButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardOverview_);

        focusDefaultButton_->SetNavigationTarget(bk::NavigationDirection::Up, openModalButton_);
        focusDefaultButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusLastButton_);
        focusDefaultButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardOverview_);
        focusLastButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusDefaultButton_);
        focusLastButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusClearButton_);
        focusLastButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardMetrics_);
        focusClearButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusLastButton_);
        focusClearButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusWireframeButton_);
        focusClearButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardOpenModal_);
        focusWireframeButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusClearButton_);
        focusWireframeButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusMotionButton_);
        focusWireframeButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        focusMotionButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusWireframeButton_);
        focusMotionButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusLayerTrailButton_);
        focusMotionButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        focusLayerTrailButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusMotionButton_);
        focusLayerTrailButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusThemeAuroraButton_);
        focusLayerTrailButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        focusThemeAuroraButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusLayerTrailButton_);
        focusThemeAuroraButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusThemeSunsetButton_);
        focusThemeAuroraButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        focusThemeSunsetButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusThemeAuroraButton_);
        focusThemeSunsetButton_->SetNavigationTarget(bk::NavigationDirection::Down, focusThemeMintButton_);
        focusThemeSunsetButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        focusThemeMintButton_->SetNavigationTarget(bk::NavigationDirection::Up, focusThemeSunsetButton_);
        focusThemeMintButton_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);

        cardOverview_->SetNavigationTarget(bk::NavigationDirection::Left, focusDefaultButton_);
        cardOverview_->SetNavigationTarget(bk::NavigationDirection::Right, cardMetrics_);
        cardOverview_->SetNavigationTarget(bk::NavigationDirection::Down, cardOpenModal_);
        cardMetrics_->SetNavigationTarget(bk::NavigationDirection::Left, cardOverview_);
        cardMetrics_->SetNavigationTarget(bk::NavigationDirection::Down, cardRestoreFocus_);
        cardMetrics_->SetNavigationTarget(bk::NavigationDirection::Right, focusLastButton_);
        cardOpenModal_->SetNavigationTarget(bk::NavigationDirection::Up, cardOverview_);
        cardOpenModal_->SetNavigationTarget(bk::NavigationDirection::Left, focusClearButton_);
        cardOpenModal_->SetNavigationTarget(bk::NavigationDirection::Right, cardRestoreFocus_);
        cardRestoreFocus_->SetNavigationTarget(bk::NavigationDirection::Up, cardMetrics_);
        cardRestoreFocus_->SetNavigationTarget(bk::NavigationDirection::Left, cardOpenModal_);
        cardRestoreFocus_->SetNavigationTarget(bk::NavigationDirection::Right, focusMotionButton_);
    }

    std::shared_ptr<bk::VBox> rootColumn_;
    std::shared_ptr<bk::ScrollView> rootScroll_;
    std::shared_ptr<bk::HBox> headerBar_;
    std::shared_ptr<bk::VBox> headerText_;
    std::shared_ptr<bk::Label> headerTitle_;
    std::shared_ptr<bk::Label> headerSubtitle_;
    std::shared_ptr<bk::Label> headerHint_;
    std::shared_ptr<bk::HBox> bodyRow_;
    std::shared_ptr<bk::VBox> leftColumn_;
    std::shared_ptr<bk::VBox> navPanel_;
    std::shared_ptr<bk::Label> navTitle_;
    std::shared_ptr<bk::Label> navTip_;
    std::shared_ptr<ActionButton> navHome_;
    std::shared_ptr<ActionButton> navGallery_;
    std::shared_ptr<ActionButton> openModalButton_;
    std::shared_ptr<bk::VBox> focusPanel_;
    std::shared_ptr<bk::Label> focusTitle_;
    std::shared_ptr<bk::Label> focusTip_;
    std::shared_ptr<ActionButton> focusDefaultButton_;
    std::shared_ptr<ActionButton> focusLastButton_;
    std::shared_ptr<ActionButton> focusClearButton_;
    std::shared_ptr<ActionButton> focusWireframeButton_;
    std::shared_ptr<ActionButton> focusMotionButton_;
    std::shared_ptr<ActionButton> focusLayerTrailButton_;
    std::shared_ptr<bk::Label> focusThemeLabel_;
    std::shared_ptr<ActionButton> focusThemeAuroraButton_;
    std::shared_ptr<ActionButton> focusThemeSunsetButton_;
    std::shared_ptr<ActionButton> focusThemeMintButton_;
    std::shared_ptr<bk::VBox> langPanel_;
    std::shared_ptr<bk::Label> langTitle_;
    std::shared_ptr<bk::Label> langHint_;
    std::shared_ptr<ActionButton> langZhButton_;
    std::shared_ptr<ActionButton> langEnButton_;
    std::shared_ptr<bk::VBox> rightColumn_;
    std::shared_ptr<bk::VBox> statusPanel_;
    std::shared_ptr<bk::Label> statusTitle_;
    std::shared_ptr<bk::Label> statusFocus_;
    std::shared_ptr<bk::Label> statusDefault_;
    std::shared_ptr<bk::Label> statusLast_;
    std::shared_ptr<bk::Label> statusPointer_;
    std::shared_ptr<bk::Label> statusKey_;
    std::shared_ptr<bk::Label> statusAction_;
    std::shared_ptr<bk::Label> statusModal_;
    std::shared_ptr<bk::HBox> lowerRow_;
    std::shared_ptr<bk::VBox> treePanel_;
    std::shared_ptr<bk::Label> treeTitle_;
    std::shared_ptr<bk::Label> treeSubtitle_;
    std::vector<std::shared_ptr<bk::Label>> treeLines_;
    std::shared_ptr<bk::VBox> actionPanel_;
    std::shared_ptr<bk::Label> actionTitle_;
    std::shared_ptr<bk::Label> actionSubtitle_;
    std::shared_ptr<bk::HBox> actionRowTop_;
    std::shared_ptr<bk::HBox> actionRowBottom_;
    std::shared_ptr<ActionButton> cardOverview_;
    std::shared_ptr<ActionButton> cardMetrics_;
    std::shared_ptr<ActionButton> cardOpenModal_;
    std::shared_ptr<ActionButton> cardRestoreFocus_;
    std::string currentFocusThemeName_{"Aurora"};
    bool openModalRequested_ = false;
    bool wireframeEnabled_ = false;
    bk::Vector2 windowSize_{kDesignWidth, kDesignHeight};
};

}

int main(int argc, char** argv)
{
    try
    {
        std::fprintf(stdout, "bkui_demo: startup begin\n");
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");
        bk::I18n::Instance().Init("i18n");
        std::fprintf(stdout, "bkui_demo: filesystem ready\n");

        bk::ApplicationHost host;
        bk::ApplicationHostDesc hostDesc;
        hostDesc.window.title = "BeikUI Focus Demo";
        hostDesc.window.width = static_cast<int>(kDesignWidth);
        hostDesc.window.height = static_cast<int>(kDesignHeight);
        hostDesc.logicalSize = bk::Vector2{kDesignWidth, kDesignHeight};
        hostDesc.clearColor = bk::Color{0.03F, 0.04F, 0.08F, 1.0F};
        hostDesc.application.name = "BeikUI Focus Demo";
        hostDesc.application.version = "0.3.0";
        hostDesc.application.identifier = "bkui.demo.focus_view";
        hostDesc.application.logger.level = bk::LogLevel::Debug;
        hostDesc.application.logger.enableConsole = true;
        hostDesc.application.logger.enableColor =
#if defined(BKUI_PLATFORM_SWITCH)
            false;
#else
            true;
#endif
        hostDesc.application.logger.flushEachMessage =
#if defined(BKUI_PLATFORM_SWITCH)
            true;
#else
            false;
#endif
        hostDesc.application.logger.filePath = "bkui_focus_demo.log";

        if (!host.Initialize(hostDesc, argc, argv))
        {
            std::fprintf(stderr, "Failed to initialize demo host.\n");
            return 1;
        }

        bk::Application& app = host.GetApplication();
        app.SetPreserveInactiveFocusHighlights(true);
        bk::Logger::instance().Info("bkui_demo: application initialized");
        app.SetFocusHighlightCornerRadius(1.0F);

        auto page = std::make_shared<DemoPage>();
        page->SetFrame(bk::Rect{0.0F, 0.0F, kDesignWidth, kDesignHeight});
        page->SetWindowSize(EnsureWindowSize(host.GetWindowSize()));
        app.AddView(page);
        page->RequestDefaultFocus();

        std::shared_ptr<DemoModalView> modal;

        host.OnResize().Connect([page](bk::ApplicationHost&, bk::Vector2 size) {
            page->SetWindowSize(EnsureWindowSize(size));
        });

        host.OnFrameEnd().Connect([&](bk::ApplicationHost&, float, std::uint64_t) {
            if (!modal && page->ConsumeOpenModalRequest())
            {
                modal = std::make_shared<DemoModalView>();
                modal->SetFrame(bk::Rect{0.0F, 0.0F, kDesignWidth, kDesignHeight});
                modal->ApplyWireframe(page->IsWireframeEnabled());
                app.AddView(modal);
                modal->RequestDefaultFocus();
            }

            if (modal && modal->ConsumeCloseRequested())
            {
                modal->ClearCurrentFocus();
                app.RemoveView(modal);
                modal.reset();
                page->RequestLastFocus();
            }
        });

        bk::MainLoopDesc loopDesc;
        loopDesc.fixedDeltaSeconds = 1.0F / 60.0F;
        loopDesc.useFixedDelta = true;
        loopDesc.synchronizeToFixedDelta = true;
        const std::uint64_t executedFrames = host.MainLoop(loopDesc);

        if (app.QuitRequested())
        {
            bk::Logger::instance().Info("bkui_demo: loop ended because application requested quit");
        }
        else if (const bk::Platform* platform = host.GetPlatform(); platform != nullptr && !platform->IsRunning())
        {
            bk::Logger::instance().Warn("bkui_demo: loop ended because platform is no longer running");
        }
        else
        {
            bk::Logger::instance().Warn("bkui_demo: loop ended unexpectedly");
        }
        bk::Logger::instance().Info("bkui_demo: executed frames = " + std::to_string(executedFrames));

        host.Shutdown();
        bk::FileSystem::Shutdown();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Fatal error: %s\n", ex.what());
        return 1;
    }

    return 0;
}
