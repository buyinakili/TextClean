#pragma once

#include "win32_base.hpp"

namespace app {
constexpr int kWindowWidth = 400, kMargin = 14, kHeaderHeight = 57;
constexpr int kInputMin = 50, kInputMax = 200, kOutputMin = 64, kOutputMax = 200;
constexpr COLORREF kGreen = RGB(45, 212, 163);
constexpr COLORREF kDarkGreen = RGB(22, 135, 101);
constexpr COLORREF kBorder = RGB(220, 232, 227);
constexpr COLORREF kSoft = RGB(239, 249, 245);
constexpr COLORREF kText = RGB(30, 42, 38);

enum ButtonId { None = 0, Settings = 1, Minimize = 2, Close = 3, ClearInput = 4, CopyOutput = 5, Color = 6, ApplyColor = 7, DeleteOutput = 8 };

struct AppState {
    HWND hwnd{}, input{}, output{}, clearOverlay{}, copyOverlay{}, deleteOverlay{}, colorInput{}, dragScroll{};
    HFONT font{}, titleFont{};
    bool settings = false, topmost = true, startup = false, colorPicker = false, copied = false;
    COLORREF theme = kGreen;
    int hot = None, pressed = None, inputHeight = kInputMin;
    RECT inputRect{}, outputRect{}, settingsRect{}, settingsButton{}, minButton{}, closeButton{}, clearButton{}, copyButton{}, deleteButton{}, startupCheck{}, topmostCheck{}, colorButton{}, applyColor{};
    std::vector<RECT> presetColors;
    std::wstring result;
};
}
