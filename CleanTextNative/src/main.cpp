#include "win32_base.hpp"
#include "selftest.hpp"
#include "svg_renderer.hpp"
#include "win32_window.hpp"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    svg::Renderer renderer;
    renderer.initialize(instance);
    win::create(instance, renderer);
    if (wcsstr(GetCommandLineW(), L"--selftest"))
        test::run();
    int rc = win::run();
    renderer.shutdown();
    return rc;
}
