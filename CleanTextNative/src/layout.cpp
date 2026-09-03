#include "layout.hpp"

namespace ui::layout
{
    RECT rect(int l, int t, int r, int b) { return RECT{l, t, r, b}; }
    bool contains(const RECT &r, POINT p) { return PtInRect(&r, p) != FALSE; }
    int heightFor(HWND edit, int minimum, int maximum)
    {
        int lines = std::max(1, int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0)));
        return std::clamp(lines * 18 + 22, minimum, maximum);
    }
    void compute(app::AppState &s, bool force)
    {
        s.darkModeButton = rect(app::kWindowWidth - 179, 13, app::kWindowWidth - 146, 39);
        s.infoButton = rect(app::kWindowWidth - 142, 13, app::kWindowWidth - 112, 39);
        s.settingsButton = rect(app::kWindowWidth - 108, 13, app::kWindowWidth - 78, 39);
        s.minButton = rect(app::kWindowWidth - 74, 13, app::kWindowWidth - 44, 39);
        s.closeButton = rect(app::kWindowWidth - 40, 13, app::kWindowWidth - 10, 39);
        if (s.infoPage)
        {
            s.settingsRect = s.inputRect = s.outputRect = rect(0, 0, 0, 0);
            s.announcementButton = rect(0, 0, 0, 0);
            s.bilibiliLink = rect(app::kMargin, 198, app::kMargin + 40, 238);
            s.githubLink = rect(app::kMargin + 46, 198, app::kMargin + 86, 238);
            s.supportButton = rect(app::kMargin, 248, app::kMargin + 112, 280);
            s.paymentImage = rect(40, 324, 360, 644);
            for (HWND control : {s.input, s.output, s.colorInput, s.filterInput, s.clearOverlay, s.copyOverlay, s.deleteOverlay}) if (control) ShowWindow(control, SW_HIDE);
            const int clientH = s.supportOpen ? 660 : 300;
            RECT client{}; GetClientRect(s.hwnd, &client);
            if (force || client.bottom != clientH) SetWindowPos(s.hwnd, nullptr, 0, 0, app::kWindowWidth, clientH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(s.hwnd, nullptr, FALSE);
            return;
        }
        int inputH = heightFor(s.input, app::kInputMin, app::kInputMax), y = app::kHeaderHeight;
        if (s.settings)
        {
            int settingsH = 104;
            {
                int itemX = app::kMargin + 16;
                int itemY = y + (s.colorPicker ? 192 : 130);
                constexpr int itemHeight = 28, itemGap = 6, inputWidth = 138;
                for (auto &item : s.filters)
                {
                    int width = item.builtin ? 32 : std::clamp(28 + int(item.text.size()) * 14, 48, 128);
                    if (itemX + width > app::kWindowWidth - app::kMargin - 16)
                    {
                        itemX = app::kMargin + 16;
                        itemY += itemHeight + itemGap;
                    }
                    item.button = rect(itemX, itemY, itemX + width, itemY + itemHeight);
                    item.deleteButton = item.builtin ? rect(0, 0, 0, 0) : rect(item.button.right - 13, item.button.top + 2, item.button.right - 2, item.button.top + 13);
                    itemX += width + itemGap;
                }
                if (itemX + inputWidth > app::kWindowWidth - app::kMargin - 16)
                {
                    itemX = app::kMargin + 16;
                    itemY += itemHeight + itemGap;
                }
                s.filterInputRect = rect(itemX, itemY, itemX + inputWidth, itemY + itemHeight);
                settingsH = s.filterInputRect.bottom - y + 16;
            }
            s.settingsRect = rect(app::kMargin, y, app::kWindowWidth - app::kMargin, y + settingsH);
            y += settingsH + 8;
        }
        else
            s.settingsRect = rect(0, 0, 0, 0);
        s.inputRect = rect(app::kMargin, y, app::kWindowWidth - app::kMargin, y + inputH);
        y += inputH + 8;
        if (!s.result.empty())
        {
            int outH = heightFor(s.output, app::kOutputMin, app::kOutputMax);
            s.outputRect = rect(app::kMargin, y, app::kWindowWidth - app::kMargin, y + outH);
            y += outH + 8;
        }
        else
            s.outputRect = rect(0, 0, 0, 0);
        int clientH = std::clamp(y + 16, 96, 620);
        RECT client{};
        GetClientRect(s.hwnd, &client);
        if (force || client.bottom != clientH)
            SetWindowPos(s.hwnd, nullptr, 0, 0, app::kWindowWidth, clientH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(s.input, SW_SHOW);
        MoveWindow(s.input, s.inputRect.left + 9, s.inputRect.top + 7, (s.inputRect.right - s.inputRect.left) - 78, (s.inputRect.bottom - s.inputRect.top) - 14, TRUE);
        ShowWindow(s.output, s.result.empty() ? SW_HIDE : SW_SHOW);
        if (!s.result.empty())
            MoveWindow(s.output, s.outputRect.left + 10, s.outputRect.top + 8, (s.outputRect.right - s.outputRect.left) - 100, (s.outputRect.bottom - s.outputRect.top) - 16, TRUE);
        s.clearButton = rect(s.inputRect.right - 51, s.inputRect.bottom - 41, s.inputRect.right - 20, s.inputRect.bottom - 10);
        s.copyButton = rect(s.outputRect.right - 51, s.outputRect.bottom - 41, s.outputRect.right - 20, s.outputRect.bottom - 10);
        // 删除按钮放在复制按钮正左侧，与 g_output 右边沿完全分离避免被白底遮挡
        s.deleteButton = rect(s.outputRect.right - 86, s.outputRect.bottom - 41, s.outputRect.right - 55, s.outputRect.bottom - 10);
        if (s.clearOverlay)
            ShowWindow(s.clearOverlay, SW_HIDE);
        if (s.copyOverlay)
            ShowWindow(s.copyOverlay, SW_HIDE);
        if (s.deleteOverlay)
            ShowWindow(s.deleteOverlay, SW_HIDE);
        s.startupCheck = rect(app::kWindowWidth - app::kMargin - 48, s.settingsRect.top + 17, app::kWindowWidth - app::kMargin - 16, s.settingsRect.top + 35);
        s.topmostCheck = rect(app::kWindowWidth - app::kMargin - 48, s.settingsRect.top + 47, app::kWindowWidth - app::kMargin - 16, s.settingsRect.top + 65);
        s.colorButton = rect(app::kWindowWidth - app::kMargin - 36, s.settingsRect.top + 75, app::kWindowWidth - app::kMargin - 16, s.settingsRect.top + 95);
        s.applyColor = rect(0, 0, 0, 0);
        s.presetColors.clear();
        if (s.colorPicker)
            for (int i = 0; i < 5; ++i)
                s.presetColors.push_back(rect(app::kMargin + 48 + i * 34, s.settingsRect.top + 104, app::kMargin + 72 + i * 34, s.settingsRect.top + 128));
        if (s.colorInput)
        {
            ShowWindow(s.colorInput, s.settings && s.colorPicker ? SW_SHOW : SW_HIDE);
            if (s.settings && s.colorPicker)
            {
                s.colorInputRect = rect(app::kMargin + 48, s.settingsRect.top + 134, app::kMargin + 228, s.settingsRect.top + 158);
                MoveWindow(s.colorInput, s.colorInputRect.left + 1, s.colorInputRect.top + 1, (s.colorInputRect.right - s.colorInputRect.left) - 2, (s.colorInputRect.bottom - s.colorInputRect.top) - 2, TRUE);
            }
            else
                s.colorInputRect = rect(0, 0, 0, 0);
        }
        if (s.filterInput)
        {
            ShowWindow(s.filterInput, s.settings ? SW_SHOW : SW_HIDE);
            if (s.settings)
                MoveWindow(s.filterInput, s.filterInputRect.left + 1, s.filterInputRect.top + 1, (s.filterInputRect.right - s.filterInputRect.left) - 2, (s.filterInputRect.bottom - s.filterInputRect.top) - 2, TRUE);
        }
        s.inputHeight = inputH;
        InvalidateRect(s.hwnd, nullptr, FALSE);
    }
}
