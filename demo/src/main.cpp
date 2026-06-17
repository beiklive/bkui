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
// 这个 demo 直接使用 i18n 字面量后缀，方便在构建控件时写出
// "key"_i18n 形式的本地化文本。
BK_MACRO_USE_I18N

// Demo 的逻辑设计尺寸。窗口真实尺寸会在 resize 时同步到页面，
// 但控件布局仍以这套 1280x720 的逻辑坐标为基准。
constexpr float kDesignWidth = 1280.0F;
constexpr float kDesignHeight = 720.0F;

// 将 View 指针转换成适合显示在状态面板里的名字：
// 空指针显示 None，未命名节点显示 Unnamed。
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

// 递归开关整棵 View 树的调试线框。线框状态不只作用在根节点，
// 子控件也需要同步，否则只会看到外层容器的边界。
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

// 带标题和副标题的 demo 按钮。
// bk::Button 默认只绘制主文本，这里覆写 Draw 额外绘制说明文本，
// 并通过 callback_ 把点击事件交给页面或模态框处理。
class ActionButton : public bk::Button
{
public:
    using Callback = std::function<void(ActionButton&)>;

    ActionButton(std::string title, std::string subtitle)
        : bk::Button(std::move(title))
        , subtitle_(std::move(subtitle))
    {
        // 统一按钮外观：较高的触控面积、圆角以及投影，便于演示焦点高亮。
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
        // 副标题变化会影响绘制内容，通知布局系统后续重新计算。
        subtitle_ = std::move(subtitle);
        InvalidateLayout();
    }

    void SetCallback(Callback callback)
    {
        callback_ = std::move(callback);
    }

    void Draw(bk::RenderQueue& queue) const override
    {
        // 先复用 Box/Button 的背景、圆角、阴影等基础绘制。
        bk::Box::Draw(queue);

        // 主标题放在上方，副标题占据下方剩余内容区。
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
        // 点击位置由框架完成命中测试；进入这里说明按钮已被点击。
        if (callback_)
        {
            callback_(*this);
        }
    }

private:
    std::string subtitle_;
    Callback callback_{};
};

bk::FocusHighlightStyle MakeFocusStyle(const bk::Color& color1, const bk::Color& color2, float inset = 6.0F)
{
    bk::FocusHighlightStyle style;
    style.color1 = color1;
    style.color2 = color2;
    style.cornerRadius = -1.0F;
    style.inset = inset;
    return style;
}

// 模态覆盖层：以最高层级盖在 DemoPage 上，展示 Top View、
// 默认焦点、最近焦点、点击外部关闭等行为。
class DemoModalView final : public bk::View
{
public:
    DemoModalView()
    {
        // 提高 z-index，确保遮罩和对话框绘制在主页面之上。
        SetName("ModalOverlay");
        SetZIndex(100);

        // 对话框本体使用 VBox，纵向排列标题、状态文本和两行按钮。
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

        title_ = MakeLabel("modal/title"_i18n, 30.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        subtitle_ = MakeLabel("modal/subtitle"_i18n, 17.0F, bk::Color{0.77F, 0.84F, 0.93F, 1.0F});
        modalFocusLabel_ = MakeLabel("", 16.0F, bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        modalDefaultLabel_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        modalLastLabel_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        modalActionLabel_ = MakeLabel("modal/action_ready"_i18n, 16.0F, bk::Color{0.98F, 0.85F, 0.50F, 1.0F});

        // 四个操作按钮分成上下两行，便于演示方向键/手柄导航。
        topRow_ = std::make_shared<bk::HBox>();
        topRow_->SetName("ModalTopRow");
        topRow_->SetSpacing(12.0F);
        topRow_->SetMarginBottom(4.0F);
        bottomRow_ = std::make_shared<bk::HBox>();
        bottomRow_->SetName("ModalBottomRow");
        bottomRow_->SetSpacing(12.0F);

        primaryButton_ = MakeButton(
            "modal/confirm"_i18n,
            "modal/confirm_hint"_i18n,
            bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        secondaryButton_ = MakeButton(
            "modal/go_default"_i18n,
            "modal/go_default_hint"_i18n,
            bk::Color{0.18F, 0.67F, 0.53F, 1.0F});
        lastFocusButton_ = MakeButton(
            "modal/go_last"_i18n,
            "modal/go_last_hint"_i18n,
            bk::Color{0.77F, 0.48F, 0.24F, 1.0F});
        closeButton_ = MakeButton(
            "modal/close"_i18n,
            "modal/close_hint"_i18n,
            bk::Color{0.78F, 0.32F, 0.36F, 1.0F});

        // 模态框内部按钮只修改自身状态或设置关闭标记；
        // 真正移除模态层由主循环在帧尾统一处理。
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

        // 组装模态对话框的 View 树。
        dialog_->AddChild(title_);
        dialog_->AddChild(subtitle_);
        dialog_->AddChild(modalFocusLabel_);
        dialog_->AddChild(modalDefaultLabel_);
        dialog_->AddChild(modalLastLabel_);
        dialog_->AddChild(modalActionLabel_);
        dialog_->AddChild(topRow_);
        dialog_->AddChild(bottomRow_);
        AddChild(dialog_);

        // 同时给覆盖层和对话框设置默认焦点，保证打开模态框后
        // RequestDefaultFocus 能稳定落到主按钮上。
        SetDefaultFocusView(primaryButton_);
        dialog_->SetDefaultFocusView(primaryButton_);

        // 显式声明模态框内的方向导航关系，避免布局变化影响焦点路径。
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
        // “消费式”读取：外部读到 true 后立即清零，避免连续多帧重复关闭。
        const bool requested = closeRequested_;
        closeRequested_ = false;
        return requested;
    }

    void ApplyWireframe(bool enabled)
    {
        // 模态层可能在主页面开启线框后才被创建，因此打开时需要同步一次。
        SetDebugWireframeRecursive(shared_from_this(), enabled);
    }

    void SyncStatus(const bk::Application& app)
    {
        // 这些文本每帧刷新，方便观察焦点在顶层 View 与模态框内部的变化。
        modalFocusLabel_->SetText("modal/focused"_i18n(DisplayName(app.GetFocusedView())));
        modalDefaultLabel_->SetText("modal/default_val"_i18n(DisplayName(GetDefaultFocusView())));
        modalLastLabel_->SetText("modal/last"_i18n(DisplayName(GetLastFocusedView())));
    }

    void Layout() override
    {
        // 覆盖层铺满窗口，内部对话框固定尺寸并居中。
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
        // 半透明遮罩压暗主页面，突出模态层。
        queue.PushRect(frame_, bk::Color{0.03F, 0.04F, 0.08F, 0.72F});
    }

    void Update(float) override
    {
        // 使用 Application::Active() 获取当前焦点状态，保持状态文本实时更新。
        if (bk::Application* app = bk::Application::Active())
        {
            SyncStatus(*app);
        }
    }

    void Click(const bk::Vector2& position) override
    {
        // 点击对话框外部等价于请求关闭；点击内部按钮则由按钮自己处理。
        if (!dialog_->ContainsPoint(position))
        {
            closeRequested_ = true;
        }
    }

private:
    static std::shared_ptr<bk::Label> MakeLabel(const std::string& text, float fontSize, const bk::Color& color)
    {
        // 小型工厂函数减少构造 Label 时重复设置字号和颜色的样板代码。
        auto label = std::make_shared<bk::Label>(text);
        label->SetFontSize(fontSize);
        label->SetTextColor(color);
        return label;
    }

    static std::shared_ptr<ActionButton> MakeButton(const std::string& title, const std::string& subtitle, const bk::Color& color)
    {
        // 模态框按钮等宽拉伸，让两列按钮保持整齐。
        auto button = std::make_shared<ActionButton>(title, subtitle);
        button->SetBackgroundColor(color);
        button->SetFlexGrow(1.0F);
        button->SetTextColor(bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
        return button;
    }

    void SetAction(std::string action)
    {
        // action 文本仍走 i18n 模板，真正的动作描述作为参数插入。
        modalActionLabel_->SetText("modal/action"_i18n(std::move(action)));
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

        // 根页面放进纵向 ScrollView，窗口高度不足时仍能访问完整演示内容。
        rootScroll_ = std::make_shared<bk::ScrollView>(bk::ScrollAxis::Vertical);
        rootScroll_->SetName("RootScroll");

        // 根列是页面的主布局容器，宽度固定为设计宽度，高度至少为设计高度。
        rootColumn_ = std::make_shared<bk::VBox>();
        rootColumn_->SetName("RootColumn");
        rootColumn_->SetPaddingTop(24.0F);
        rootColumn_->SetPaddingRight(24.0F);
        rootColumn_->SetPaddingBottom(24.0F);
        rootColumn_->SetPaddingLeft(24.0F);
        rootColumn_->SetSpacing(20.0F);
        rootColumn_->SetWidth(kDesignWidth);
        rootColumn_->SetMinHeight(kDesignHeight);

        // 顶部标题栏：左侧是标题说明，右侧是操作提示。
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
        headerTitle_ = MakeLabel("page/title"_i18n, 33.0F, bk::Color{0.98F, 0.99F, 1.0F, 1.0F});
        headerSubtitle_ = MakeLabel("page/subtitle"_i18n, 17.0F, bk::Color{0.77F, 0.84F, 0.93F, 1.0F});
        headerText_->AddChild(headerTitle_);
        headerText_->AddChild(headerSubtitle_);
        headerHint_ = MakeLabel("page/hint"_i18n, 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
        headerHint_->SetWidth(360.0F);
        headerBar_->AddChild(headerText_);
        headerBar_->AddChild(headerHint_);

        // 页面主体分左右两列：左侧偏控制区，右侧偏状态和内容区。
        bodyRow_ = std::make_shared<bk::HBox>();
        bodyRow_->SetName("BodyRow");
        bodyRow_->SetSpacing(20.0F);
        bodyRow_->SetFlexGrow(1.0F);

        leftColumn_ = std::make_shared<bk::VBox>();
        leftColumn_->SetName("LeftColumn");
        leftColumn_->SetWidth(320.0F);
        leftColumn_->SetSpacing(18.0F);

        // 导航面板演示普通按钮点击、焦点路径和打开模态层。
        navPanel_ = MakePanel("NavPanel", bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        navTitle_ = MakeLabel("nav/title"_i18n, 24.0F, bk::Color{0.10F, 0.14F, 0.21F, 1.0F});
        navTip_ = MakeLabel("nav/tip"_i18n, 16.0F, bk::Color{0.31F, 0.38F, 0.49F, 1.0F});
        navHome_ = MakeActionButton("nav/home"_i18n, "nav/home_hint"_i18n, bk::Color{0.22F, 0.49F, 0.93F, 1.0F});
        navGallery_ = MakeActionButton("nav/gallery"_i18n, "nav/gallery_hint"_i18n, bk::Color{0.20F, 0.64F, 0.55F, 1.0F});
        openModalButton_ = MakeActionButton("nav/open_modal"_i18n, "nav/open_modal_hint"_i18n, bk::Color{0.79F, 0.45F, 0.26F, 1.0F});
        openModalButton_->SetMarginTop(4.0F);
        navPanel_->AddChild(navTitle_);
        navPanel_->AddChild(navTip_);
        navPanel_->AddChild(navHome_);
        navPanel_->AddChild(navGallery_);
        navPanel_->AddChild(openModalButton_);

        // 焦点面板集中演示默认焦点、最近焦点、清空焦点、线框和焦点高亮效果。
        focusPanel_ = MakePanel("FocusPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        focusTitle_ = MakeLabel("focus/title"_i18n, 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        focusTip_ = MakeLabel("focus/tip"_i18n, 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        focusDefaultButton_ = MakeActionButton("focus/go_default"_i18n, "focus/go_default_hint"_i18n, bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        focusLastButton_ = MakeActionButton("focus/go_last"_i18n, "focus/go_last_hint"_i18n, bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        focusClearButton_ = MakeActionButton("focus/clear"_i18n, "focus/clear_hint"_i18n, bk::Color{0.72F, 0.34F, 0.39F, 1.0F});
        focusWireframeButton_ = MakeActionButton("focus/wireframe_off"_i18n, "focus/wireframe_hint"_i18n, bk::Color{0.43F, 0.40F, 0.86F, 1.0F});
        focusMotionButton_ = MakeActionButton("focus/motion_on"_i18n, "focus/motion_hint"_i18n, bk::Color{0.86F, 0.36F, 0.72F, 1.0F});
        focusLayerTrailButton_ = MakeActionButton("focus/trail_on"_i18n, "focus/trail_hint"_i18n, bk::Color{0.34F, 0.62F, 0.88F, 1.0F});
        focusThemeLabel_ = MakeLabel("focus/theme_label"_i18n("Aurora"), 15.0F, bk::Color{0.84F, 0.89F, 0.97F, 1.0F});
        focusThemeAuroraButton_ = MakeActionButton("focus/theme_aurora"_i18n, "focus/theme_aurora_hint"_i18n, bk::Color{0.24F, 0.52F, 0.95F, 1.0F});
        focusThemeSunsetButton_ = MakeActionButton("focus/theme_sunset"_i18n, "focus/theme_sunset_hint"_i18n, bk::Color{0.92F, 0.46F, 0.34F, 1.0F});
        focusThemeMintButton_ = MakeActionButton("focus/theme_mint"_i18n, "focus/theme_mint_hint"_i18n, bk::Color{0.22F, 0.72F, 0.64F, 1.0F});
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

        // 语言面板演示运行时切换 i18n 文案。
        langPanel_ = MakePanel("LangPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        langTitle_ = MakeLabel("lang/title"_i18n, 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        langHint_ = MakeLabel("lang/hint"_i18n, 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        langZhButton_ = MakeActionButton("lang/zh"_i18n, "lang/zh_hint"_i18n, bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        langEnButton_ = MakeActionButton("lang/en"_i18n, "lang/en_hint"_i18n, bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        langPanel_->AddChild(langTitle_);
        langPanel_->AddChild(langHint_);
        langPanel_->AddChild(langZhButton_);
        langPanel_->AddChild(langEnButton_);

        // 左列的三个面板按控制类别从上到下排列。
        leftColumn_->AddChild(navPanel_);
        leftColumn_->AddChild(focusPanel_);
        leftColumn_->AddChild(langPanel_);

        // 右列展示实时状态、View 树和内容区动作按钮。
        rightColumn_ = std::make_shared<bk::VBox>();
        rightColumn_->SetName("RightColumn");
        rightColumn_->SetFlexGrow(1.0F);
        rightColumn_->SetSpacing(18.0F);

        // 状态面板显示当前焦点、默认焦点、最近焦点、指针位置、输入事件和顶层 View 数量。
        statusPanel_ = MakePanel("StatusPanel", bk::Color{0.11F, 0.15F, 0.23F, 1.0F});
        statusTitle_ = MakeLabel("status/title"_i18n, 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        statusFocus_ = MakeLabel("", 16.0F, bk::Color{0.94F, 0.96F, 1.0F, 1.0F});
        statusDefault_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusLast_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusPointer_ = MakeLabel("", 16.0F, bk::Color{0.80F, 0.86F, 0.95F, 1.0F});
        statusKey_ = MakeLabel("status/key_idle"_i18n, 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
        statusAction_ = MakeLabel("status/action_ready"_i18n, 16.0F, bk::Color{0.98F, 0.85F, 0.51F, 1.0F});
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

        // View 树面板用于观察当前 Application 中可见 View 的层级关系。
        treePanel_ = MakePanel("TreePanel", bk::Color{0.93F, 0.95F, 0.99F, 1.0F});
        treePanel_->SetWidth(338.0F);
        treeTitle_ = MakeLabel("tree/title"_i18n, 23.0F, bk::Color{0.10F, 0.14F, 0.21F, 1.0F});
        treeSubtitle_ = MakeLabel("tree/subtitle"_i18n, 16.0F, bk::Color{0.31F, 0.38F, 0.49F, 1.0F});
        treePanel_->AddChild(treeTitle_);
        treePanel_->AddChild(treeSubtitle_);
        // 预先创建固定数量的文本行，刷新时只替换内容，避免每帧增删控件。
        for (int index = 0; index < 16; ++index)
        {
            auto line = MakeLabel("", 15.0F, bk::Color{0.16F, 0.22F, 0.32F, 1.0F});
            line->SetMarginLeft(4.0F);
            treeLines_.push_back(line);
            treePanel_->AddChild(line);
        }

        // 内容动作区提供另一组可聚焦按钮，用来测试跨区域方向导航。
        actionPanel_ = MakePanel("ActionPanel", bk::Color{0.14F, 0.18F, 0.28F, 1.0F});
        actionPanel_->SetFlexGrow(1.0F);
        actionTitle_ = MakeLabel("action/title"_i18n, 24.0F, bk::Color{0.97F, 0.98F, 1.0F, 1.0F});
        actionSubtitle_ = MakeLabel("action/subtitle"_i18n, 16.0F, bk::Color{0.78F, 0.84F, 0.93F, 1.0F});
        actionRowTop_ = std::make_shared<bk::HBox>();
        actionRowTop_->SetName("ActionRowTop");
        actionRowTop_->SetSpacing(12.0F);
        actionRowBottom_ = std::make_shared<bk::HBox>();
        actionRowBottom_->SetName("ActionRowBottom");
        actionRowBottom_->SetSpacing(12.0F);

        cardOverview_ = MakeActionButton("action/overview"_i18n, "action/overview_hint"_i18n, bk::Color{0.23F, 0.50F, 0.95F, 1.0F});
        cardMetrics_ = MakeActionButton("action/metrics"_i18n, "action/metrics_hint"_i18n, bk::Color{0.20F, 0.68F, 0.52F, 1.0F});
        cardOpenModal_ = MakeActionButton("action/open_modal"_i18n, "action/open_modal_hint"_i18n, bk::Color{0.79F, 0.45F, 0.26F, 1.0F});
        cardRestoreFocus_ = MakeActionButton("action/restore_focus"_i18n, "action/restore_focus_hint"_i18n, bk::Color{0.44F, 0.41F, 0.86F, 1.0F});

        // 动作按钮按两行两列摆放，和左侧控制面板形成横向导航关系。
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

        // 首次进入页面时默认聚焦左上角的 Home 按钮。
        SetDefaultFocusView(navHome_);

        // 构造控件后再统一绑定事件和方向导航，避免成员尚未初始化时互相引用。
        BindCallbacks();
        BuildNavigation();
    }

    void SyncStatus(const bk::Application& app, const bk::InputState& input, const bk::Vector2& windowSize)
    {
        // 触摸优先显示第一个触点位置；没有触摸时显示鼠标位置。
        const bk::Vector2 pointer = input.touchCount > 0 ? input.touchPoints[0].position : input.mousePosition;
        statusFocus_->SetText("status/focused"_i18n(DisplayName(app.GetFocusedView())));
        statusDefault_->SetText("status/default_val"_i18n(DisplayName(GetDefaultFocusView())));
        statusLast_->SetText("status/last"_i18n(DisplayName(GetLastFocusedView())));
        statusPointer_->SetText(
            "status/pointer"_i18n(
                demo::FormatFloat(pointer.x, 0),
                demo::FormatFloat(pointer.y, 0),
                std::to_string(static_cast<int>(windowSize.x)),
                std::to_string(static_cast<int>(windowSize.y))));
        statusModal_->SetText("status/top_views"_i18n(std::to_string(app.GetViews().size())));

        // 只在输入状态发生变化时更新提示文本，避免空闲帧覆盖最后一次输入提示。
        if (input.keyPressed)
        {
            statusKey_->SetText("status/key_pressed"_i18n(input.lastKeyEvent.name));
        }
        else if (input.keyReleased)
        {
            statusKey_->SetText("status/key_released"_i18n(input.lastKeyEvent.name));
        }
        else if (input.mouseLeftPressed)
        {
            statusKey_->SetText("status/key_mouse"_i18n);
        }
        else if (input.touchCount > 0 && input.touchPoints[0].pressed)
        {
            statusKey_->SetText("status/key_touch"_i18n);
        }

        // 构造当前可见 View 树的简略文本，每层用两个空格缩进，
        // 聚焦节点额外标记 [F]。
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

        // Application 的顶层 View 可能包含主页面和模态层，因此从 app.GetViews() 开始遍历。
        for (const auto& rootView : app.GetViews())
        {
            appendTree(rootView, 0, appendTree);
            if (lines.size() >= treeLines_.size())
            {
                break;
            }
        }

        // 没有内容的预留行清空文本，防止上一帧的树节点残留。
        for (std::size_t index = 0; index < treeLines_.size(); ++index)
        {
            treeLines_[index]->SetText(index < lines.size() ? lines[index] : "");
        }
    }

    bool ConsumeOpenModalRequest()
    {
        // 页面只发出“想打开模态框”的请求，实际创建由 main 的帧尾回调完成。
        const bool requested = openModalRequested_;
        openModalRequested_ = false;
        return requested;
    }

    bool IsWireframeEnabled() const
    {
        return wireframeEnabled_;
    }

    void ApplyWireframe(bool enabled)
    {
        // 记录全局线框状态，并同步到当前页面的所有子控件。
        wireframeEnabled_ = enabled;
        SetDebugWireframeRecursive(shared_from_this(), enabled);
        focusWireframeButton_->SetText(enabled ? "focus/wireframe_on"_i18n : "focus/wireframe_off"_i18n);
    }

    void Layout() override
    {
        // ScrollView 占满整个页面，内部 rootColumn 由 ScrollView 管理滚动内容尺寸。
        rootScroll_->SetFrame(frame_);
        rootScroll_->Layout();
        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        // 绘制深色背景和顶部强调线，其他面板由各自控件绘制。
        queue.PushRect(frame_, bk::Color{0.05F, 0.07F, 0.11F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x, frame_.y, frame_.width, 8.0F},
            bk::Color{0.20F, 0.52F, 0.97F, 1.0F});
    }

    void Update(float) override
    {
        // 每帧刷新状态面板和会被外部开关影响的按钮文案。
        if (bk::Application* app = bk::Application::Active())
        {
            SyncStatus(*app, app->GetInputState(), app->GetWindowSize());
            SyncFocusMotionButton();
            SyncFocusLayerTrailButton();
            SyncFocusThemeLabel();
        }
    }

private:
    static std::shared_ptr<bk::Label> MakeLabel(const std::string& text, float fontSize, const bk::Color& color)
    {
        // 通用标签工厂：统一字号和颜色设置。
        auto label = std::make_shared<bk::Label>(text);
        label->SetFontSize(fontSize);
        label->SetTextColor(color);
        return label;
    }

    static std::shared_ptr<bk::VBox> MakePanel(const std::string& name, const bk::Color& color)
    {
        // 面板统一采用圆角、阴影和内边距，保证整体视觉节奏一致。
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
        // 页面内的按钮统一走这个工厂，保证主文本、副文本和配色一致。
        auto button = std::make_shared<ActionButton>(title, subtitle);
        button->SetBackgroundColor(color);
        button->SetTextColor(bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
        return button;
    }

    void SetAction(std::string action)
    {
        // 把最后一次动作写入状态栏，帮助观察点击路径是否符合预期。
        statusAction_->SetText("status/action"_i18n(std::move(action)));
    }

    void ApplyFocusTheme(const std::string& name, const bk::Color& color1, const bk::Color& color2)
    {
        // 切换当前主题名，并同步到每个可聚焦控件自己的聚焦框样式。
        currentFocusThemeName_ = name;
        for (const auto& button : FocusStyledButtons())
        {
            button->SetFocusHighlightStyle(MakeFocusStyle(color1, color2));
        }
        SyncFocusThemeLabel();
    }

    std::array<std::shared_ptr<ActionButton>, 18> FocusStyledButtons() const
    {
        return {
            navHome_,
            navGallery_,
            openModalButton_,
            focusDefaultButton_,
            focusLastButton_,
            focusClearButton_,
            focusWireframeButton_,
            focusMotionButton_,
            focusLayerTrailButton_,
            focusThemeAuroraButton_,
            focusThemeSunsetButton_,
            focusThemeMintButton_,
            langZhButton_,
            langEnButton_,
            cardOverview_,
            cardMetrics_,
            cardOpenModal_,
            cardRestoreFocus_,
        };
    }

    void SyncFocusMotionButton()
    {
        // 按钮标题会根据全局开关切换 on/off，避免用户看不出当前状态。
        if (bk::Application* app = bk::Application::Active())
        {
            const bool enabled = app->IsFocusHighlightMotionEnabled();
            focusMotionButton_->SetText(enabled ? "focus/motion_on"_i18n : "focus/motion_off"_i18n);
        }
    }

    void SyncFocusLayerTrailButton()
    {
        // 同步“保留非激活焦点高亮”开关的按钮文案。
        if (bk::Application* app = bk::Application::Active())
        {
            const bool enabled = app->IsPreservingInactiveFocusHighlights();
            focusLayerTrailButton_->SetText(enabled ? "focus/trail_on"_i18n : "focus/trail_off"_i18n);
        }
    }

    void SyncFocusThemeLabel()
    {
        // 主题标签显示当前焦点高亮主题名。
        focusThemeLabel_->SetText("focus/theme_label"_i18n(currentFocusThemeName_));
    }

    void RefreshTexts()
    {
        // 切换语言后，重新把所有静态文案刷一遍。
        headerTitle_->SetText("page/title"_i18n);
        headerSubtitle_->SetText("page/subtitle"_i18n);
        headerHint_->SetText("page/hint"_i18n);
        navTitle_->SetText("nav/title"_i18n);
        navTip_->SetText("nav/tip"_i18n);
        navHome_->SetText("nav/home"_i18n);
        navGallery_->SetText("nav/gallery"_i18n);
        openModalButton_->SetText("nav/open_modal"_i18n);
        focusTitle_->SetText("focus/title"_i18n);
        focusTip_->SetText("focus/tip"_i18n);
        focusDefaultButton_->SetText("focus/go_default"_i18n);
        focusLastButton_->SetText("focus/go_last"_i18n);
        focusClearButton_->SetText("focus/clear"_i18n);
        focusThemeAuroraButton_->SetText("focus/theme_aurora"_i18n);
        focusThemeSunsetButton_->SetText("focus/theme_sunset"_i18n);
        focusThemeMintButton_->SetText("focus/theme_mint"_i18n);
        SyncFocusThemeLabel();
        SyncFocusMotionButton();
        SyncFocusLayerTrailButton();
        ApplyWireframe(wireframeEnabled_);
        statusTitle_->SetText("status/title"_i18n);
        statusKey_->SetText("status/key_idle"_i18n);
        statusAction_->SetText("status/action_ready"_i18n);
        treeTitle_->SetText("tree/title"_i18n);
        treeSubtitle_->SetText("tree/subtitle"_i18n);
        actionTitle_->SetText("action/title"_i18n);
        actionSubtitle_->SetText("action/subtitle"_i18n);
        cardOverview_->SetText("action/overview"_i18n);
        cardMetrics_->SetText("action/metrics"_i18n);
        cardOpenModal_->SetText("action/open_modal"_i18n);
        cardRestoreFocus_->SetText("action/restore_focus"_i18n);
        langTitle_->SetText("lang/title"_i18n);
        langHint_->SetText("lang/hint"_i18n);
        langZhButton_->SetText("lang/zh"_i18n);
        langEnButton_->SetText("lang/en"_i18n);
    }

    void BindCallbacks()
    {
        // 把页面中所有按钮的“点击后做什么”集中放在这里，便于一眼查看交互逻辑。
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

        navHome_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.22F, 0.49F, 0.93F, 1.0F}, bk::Color{0.40F, 0.68F, 1.0F, 1.0F}));
        navGallery_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.20F, 0.64F, 0.55F, 1.0F}, bk::Color{0.48F, 0.88F, 0.70F, 1.0F}));
        openModalButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.79F, 0.45F, 0.26F, 1.0F}, bk::Color{0.96F, 0.66F, 0.40F, 1.0F}));
        focusDefaultButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.23F, 0.50F, 0.95F, 1.0F}, bk::Color{0.62F, 0.76F, 1.0F, 1.0F}));
        focusLastButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.20F, 0.68F, 0.52F, 1.0F}, bk::Color{0.49F, 0.88F, 0.70F, 1.0F}));
        focusClearButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.72F, 0.34F, 0.39F, 1.0F}, bk::Color{0.96F, 0.58F, 0.63F, 1.0F}));
        focusWireframeButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.43F, 0.40F, 0.86F, 1.0F}, bk::Color{0.69F, 0.66F, 1.0F, 1.0F}));
        focusMotionButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.86F, 0.36F, 0.72F, 1.0F}, bk::Color{0.99F, 0.56F, 0.88F, 1.0F}));
        focusLayerTrailButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.34F, 0.62F, 0.88F, 1.0F}, bk::Color{0.58F, 0.82F, 1.0F, 1.0F}));
        focusThemeAuroraButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.24F, 0.52F, 0.95F, 1.0F}, bk::Color{0.63F, 0.78F, 1.0F, 1.0F}));
        focusThemeSunsetButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.92F, 0.46F, 0.34F, 1.0F}, bk::Color{1.0F, 0.66F, 0.52F, 1.0F}));
        focusThemeMintButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.22F, 0.72F, 0.64F, 1.0F}, bk::Color{0.50F, 0.94F, 0.83F, 1.0F}));
        langZhButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.23F, 0.50F, 0.95F, 1.0F}, bk::Color{0.62F, 0.76F, 1.0F, 1.0F}));
        langEnButton_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.20F, 0.68F, 0.52F, 1.0F}, bk::Color{0.49F, 0.88F, 0.70F, 1.0F}));
        cardOverview_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.23F, 0.50F, 0.95F, 1.0F}, bk::Color{0.62F, 0.76F, 1.0F, 1.0F}));
        cardMetrics_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.20F, 0.68F, 0.52F, 1.0F}, bk::Color{0.49F, 0.88F, 0.70F, 1.0F}));
        cardOpenModal_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.79F, 0.45F, 0.26F, 1.0F}, bk::Color{0.96F, 0.66F, 0.40F, 1.0F}));
        cardRestoreFocus_->SetFocusHighlightStyle(MakeFocusStyle(bk::Color{0.44F, 0.41F, 0.86F, 1.0F}, bk::Color{0.70F, 0.67F, 1.0F, 1.0F}));
    }

    void BuildNavigation()
    {
        // 明确指定每个按钮在上下左右方向上的邻居，形成可预测的焦点图。
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
};

}

int main(int argc, char** argv)
{
    try
    {
        // Host 负责窗口、平台和 Application 的创建与驱动。
        bk::ApplicationHost host;
        bk::ApplicationDesc appDesc;
        appDesc.window.title = "BeikUI Focus Demo";
        appDesc.window.width = static_cast<int>(kDesignWidth);
        appDesc.window.height = static_cast<int>(kDesignHeight);
        appDesc.logicalSize = bk::Vector2{kDesignWidth, kDesignHeight};
        appDesc.clearColor = bk::Color{0.03F, 0.04F, 0.08F, 1.0F};
        appDesc.name = "BeikUI Focus Demo";
        appDesc.version = "0.3.0";
        appDesc.identifier = "bkui.demo.focus_view";
        appDesc.logger.level = bk::LogLevel::Debug;
        appDesc.logger.enableConsole = true;
        appDesc.logger.enableColor =
#if defined(BKUI_PLATFORM_SWITCH)
            false;
#else
            true;
#endif
        appDesc.logger.flushEachMessage =
#if defined(BKUI_PLATFORM_SWITCH)
            true;
#else
            false;
#endif
        appDesc.logger.filePath = "bkui_focus_demo.log";

        if (!host.Initialize(appDesc, argc, argv))
        {
            std::fprintf(stderr, "Failed to initialize demo host.\n");
            return 1;
        }

        bk::Application& app = host.GetApplication();
        app.SetPreserveInactiveFocusHighlights(true);
        bk::Logger::instance().Info("bkui_demo: application initialized");

        // 主页面是常驻层，先加入 Application，并请求默认焦点。
        auto page = std::make_shared<DemoPage>();
        app.AddView(page);
        page->RequestDefaultFocus();

        std::shared_ptr<DemoModalView> modal;

        // 帧尾统一处理“创建模态框”和“关闭模态框”，避免在回调中直接改 View 集合。
        host.OnFrameEnd().Connect([&](bk::ApplicationHost&, float, std::uint64_t) {
            if (!modal && page->ConsumeOpenModalRequest())
            {
                // 只有在页面确实请求时才创建模态层。
                modal = std::make_shared<DemoModalView>();
                modal->ApplyWireframe(page->IsWireframeEnabled());
                app.AddView(modal);
                modal->RequestDefaultFocus();
            }

            if (modal && modal->ConsumeCloseRequested())
            {
                // 关闭前先清理焦点，再从 Application 中移除模态层。
                modal->ClearCurrentFocus();
                app.RemoveView(modal);
                modal.reset();
                page->RequestLastFocus();
            }
        });

        // 主循环采用固定 60 FPS，确保演示中的焦点状态和动画节奏稳定。
        bk::MainLoopDesc loopDesc;
        loopDesc.fixedDeltaSeconds = 1.0F / 120.0F;
        loopDesc.useFixedDelta = true;
        loopDesc.synchronizeToFixedDelta = true;


        const std::uint64_t executedFrames = host.MainLoop(loopDesc);

        // 根据退出原因写不同日志，方便排查平台关闭还是应用主动退出。
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

        // 正常收尾：先停 Host，再关闭文件系统。
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
