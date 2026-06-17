#include <bkui/bkui.hpp>

BK_MACRO_USE_I18N




int main(int argc, char** argv)
{
    bk::Logger::instance().SetLogLevel(bk::Logger::LogLevel::Info);
    bk::Logger::instance().Info("bkui_demo: starting up");

    bk::ApplicationHost host;
    if (!host.Initialize(argc, argv))
    {
        bk::Logger::instance().Error("bkui_demo: failed to initialize application host");
        return 1;
    }

    return demo::RunDemo(host);
}