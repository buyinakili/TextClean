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

struct FilterItem {
    std::wstring text;
    bool selected = false;
    bool builtin = false;
    RECT button{};
    RECT deleteButton{};
};

struct AppState {
    HWND hwnd{}, input{}, output{}, clearOverlay{}, copyOverlay{}, deleteOverlay{}, colorInput{}, filterInput{}, dragScroll{};
    HFONT font{}, titleFont{};
    bool settings = false, topmost = true, startup = false, colorPicker = false, copied = false, compact = false, bubblePointerDown = false, bubbleMoved = false;
    COLORREF theme = kGreen;
    int hot = None, pressed = None, inputHeight = kInputMin, hotFilter = -1;
    RECT inputRect{}, outputRect{}, settingsRect{}, settingsButton{}, minButton{}, closeButton{}, clearButton{}, copyButton{}, deleteButton{}, startupCheck{}, topmostCheck{}, colorButton{}, applyColor{}, filterInputRect{};
    POINT bubbleStartCursor{}, bubbleStartWindow{};
    SIZE expandedSize{};
    std::vector<RECT> presetColors;
    std::vector<FilterItem> filters{{L"*", true, true}, {L"#", false, true}, {L"\\", false, true}};
    std::wstring result;
};
}
