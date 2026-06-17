#include <bkui/bkui.hpp>

BK_MACRO_USE_I18N




int main(int argc, char** argv)
{
    bklog.SetLevel(bk::LogLevel::Info);
    bklog.info("bkui_demo: starting up");

    bk::ApplicationHost host;
    if (!host.Initialize(argc, argv))
    {
        bklog.error("bkui_demo: failed to initialize application host");
        return 1;
    }

    return demo::RunDemo(host);
}