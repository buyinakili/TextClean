#pragma once
#include "win32_base.hpp"
namespace svg {
enum class Asset { Logo, Cancel, Copy, Settings, Info, Bilibili, Github };
class Renderer {
public:
    struct Impl;
    bool initialize(HINSTANCE); void shutdown(); void draw(HDC, Asset, RECT, COLORREF, bool themed); void drawPayment(HDC, RECT);
private:
    Impl* impl_{};
};
}
