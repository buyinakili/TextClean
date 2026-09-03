#include "paint.hpp"
#include "layout.hpp"
#include "graphics_backend.hpp"

namespace ui::paint
{
    using namespace app;

    // ---------- GDI+ 抗锯齿 helper（用于 toggle / 主题色按钮 / 预设圆） ----------
    // Graphics 类没有 FillRoundRect，用 GraphicsPath 拼一个。
    static void fillAntialiasedEllipse(HDC dc, RECT r, COLORREF color)
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
        g.FillEllipse(&brush, (Gdiplus::REAL)r.left, (Gdiplus::REAL)r.top,
                      (Gdiplus::REAL)(r.right - r.left), (Gdiplus::REAL)(r.bottom - r.top));
    }
    static void fillAntialiasedRoundRect(HDC dc, RECT r, int radius, COLORREF color)
    {
        Gdiplus::GraphicsPath path;
        int d = std::min(radius * 2, std::min(int(r.right - r.left), int(r.bottom - r.top)));
        path.AddArc((Gdiplus::REAL)r.left, (Gdiplus::REAL)r.top, (Gdiplus::REAL)d, (Gdiplus::REAL)d, 180, 90);
        path.AddArc((Gdiplus::REAL)(r.right - d), (Gdiplus::REAL)r.top, (Gdiplus::REAL)d, (Gdiplus::REAL)d, 270, 90);
        path.AddArc((Gdiplus::REAL)(r.right - d), (Gdiplus::REAL)(r.bottom - d), (Gdiplus::REAL)d, (Gdiplus::REAL)d, 0, 90);
        path.AddArc((Gdiplus::REAL)r.left, (Gdiplus::REAL)(r.bottom - d), (Gdiplus::REAL)d, (Gdiplus::REAL)d, 90, 90);
        path.CloseFigure();
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
        g.FillPath(&brush, &path);
    }
    static COLORREF lightTheme(COLORREF color)
    {
        constexpr int themeWeight = 18;
        constexpr int whiteWeight = 100 - themeWeight;
        return RGB((GetRValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetGValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetBValue(color) * themeWeight + 255 * whiteWeight) / 100);
    }
    static COLORREF settingsTheme(COLORREF color)
    {
        constexpr int themeWeight = 10;
        constexpr int whiteWeight = 100 - themeWeight;
        return RGB((GetRValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetGValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetBValue(color) * themeWeight + 255 * whiteWeight) / 100);
    }
    static COLORREF settingsBorder(COLORREF color)
    {
        constexpr int themeWeight = 30;
        constexpr int whiteWeight = 100 - themeWeight;
        return RGB((GetRValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetGValue(color) * themeWeight + 255 * whiteWeight) / 100,
                   (GetBValue(color) * themeWeight + 255 * whiteWeight) / 100);
    }
    static COLORREF mix(COLORREF base, COLORREF tint, int tintWeight)
    {
        const int baseWeight = 100 - tintWeight;
        return RGB((GetRValue(base) * baseWeight + GetRValue(tint) * tintWeight) / 100,
                   (GetGValue(base) * baseWeight + GetGValue(tint) * tintWeight) / 100,
                   (GetBValue(base) * baseWeight + GetBValue(tint) * tintWeight) / 100);
    }
    static COLORREF surface(const AppState &s) { return s.darkMode ? RGB(31, 35, 40) : RGB(255, 255, 255); }
    static COLORREF darkSettingsTheme(COLORREF theme) { return mix(RGB(31, 35, 40), theme, 28); }
    void paintCompactLayered(HWND hwnd, svg::Renderer &renderer, COLORREF theme, bool darkMode)
    {
        RECT client{};
        GetClientRect(hwnd, &client);
        const int width = client.right, height = client.bottom;
        if (width <= 0 || height <= 0)
            return;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        HDC screen = GetDC(nullptr);
        HDC memory = CreateCompatibleDC(screen);
        BYTE *bits{};
        HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits), nullptr, 0);
        HGDIOBJ old = SelectObject(memory, bitmap);
        ZeroMemory(bits, size_t(width) * height * 4);
        {
            Gdiplus::Graphics graphics(memory);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            Gdiplus::SolidBrush brush(darkMode ? Gdiplus::Color(255, 31, 35, 40) : Gdiplus::Color(255, 255, 255, 255));
            graphics.FillEllipse(&brush, 0.0f, 0.0f, static_cast<Gdiplus::REAL>(width), static_cast<Gdiplus::REAL>(height));
        }
        renderer.draw(memory, svg::Asset::Logo, layout::rect(11, 11, width - 11, height - 11), theme, true);
        // UpdateLayeredWindow consumes premultiplied alpha. Explicitly mask every
        // pixel outside the circle so no ordinary-window backing can show through.
        const float centerX = width * 0.5f, centerY = height * 0.5f, radius = std::min(width, height) * 0.5f;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                BYTE *pixel = bits + (size_t(y) * width + x) * 4;
                float dx = (x + 0.5f) - centerX, dy = (y + 0.5f) - centerY;
                BYTE mask = static_cast<BYTE>(std::clamp((radius + 0.5f - std::sqrt(dx * dx + dy * dy)) * 255.0f, 0.0f, 255.0f));
                BYTE oldAlpha = pixel[3], alpha = std::min(oldAlpha, mask);
                if (oldAlpha && alpha != oldAlpha)
                {
                    pixel[0] = BYTE(unsigned(pixel[0]) * alpha / oldAlpha);
                    pixel[1] = BYTE(unsigned(pixel[1]) * alpha / oldAlpha);
                    pixel[2] = BYTE(unsigned(pixel[2]) * alpha / oldAlpha);
                }
                pixel[3] = alpha;
            }
        RECT bounds{};
        GetWindowRect(hwnd, &bounds);
        POINT destination{bounds.left, bounds.top}, source{};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(hwnd, screen, &destination, &size, memory, &source, 0, &blend, ULW_ALPHA);
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
    }

    void fillRound(HDC dc, RECT r, COLORREF color, int radius, COLORREF stroke)
    {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, stroke == CLR_INVALID ? color : stroke);
        HGDIOBJ ob = SelectObject(dc, brush), op = SelectObject(dc, pen);
        RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(brush);
        DeleteObject(pen);
    }
    static void text(HDC dc, const wchar_t *value, RECT r, UINT flags, COLORREF color, HFONT font)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        HGDIOBJ old = SelectObject(dc, font);
        DrawTextW(dc, value, -1, &r, flags);
        SelectObject(dc, old);
    }
    static RECT iconBox(const RECT &r)
    {
        int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
        return layout::rect(cx - 11, cy - 11, cx + 11, cy + 11);
    }
    RECT scrollTrack(const RECT &card)
    {
        return layout::rect(card.right - 14, card.top + 7, card.right - 6, card.bottom - 7);
    }
    static int visibleLines(const RECT &card)
    {
        return std::max<int>(1, int((card.bottom - card.top - 14) / 18));
    }
    static RECT scrollThumb(HWND edit, const RECT &card)
    {
        RECT t = scrollTrack(card);
        int total = std::max(1, int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0))),
            page = visibleLines(card),
            th = std::max<int>(28, (t.bottom - t.top) * page / total),
            travel = std::max<int>(0, (t.bottom - t.top) - th),
            maxFirst = std::max<int>(1, total - page),
            first = std::clamp<int>(int(SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0)), 0, maxFirst),
            top = t.top + first * travel / maxFirst;
        return layout::rect(t.left, top, t.right, top + th);
    }
    bool hasScroll(const AppState &s, HWND edit)
    {
        return int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0)) > visibleLines(edit == s.input ? s.inputRect : s.outputRect);
    }
    static void paintScroll(HDC dc, const AppState &s, HWND edit, const RECT &card)
    {
        if (!IsWindowVisible(edit) || !hasScroll(s, edit))
            return;
        RECT t = scrollTrack(card);
        fillRound(dc, t, RGB(229, 241, 236), 8);
        fillAntialiasedRoundRect(dc, scrollThumb(edit, card), 4, s.theme); // GDI+ 抗锯齿
    }
    void scrollTo(AppState &s, HWND edit, const RECT &card, int y)
    {
        int total = std::max(1, int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0))),
            page = visibleLines(card),
            maxFirst = std::max(0, total - page),
            first = int(SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
        RECT t = scrollTrack(card), thumb = scrollThumb(edit, card);
        int travel = std::max<int>(1, (t.bottom - t.top) - (thumb.bottom - thumb.top));
        int target = std::clamp<int>((y - t.top - (thumb.bottom - thumb.top) / 2) * maxFirst / travel, 0, maxFirst);
        SendMessageW(edit, EM_LINESCROLL, 0, target - first);
        InvalidateRect(s.hwnd, &card, FALSE);
    }
    // 抗锯齿 toggle 开关：圆角矩形底 + 圆形滑块
    static void toggle(HDC dc, RECT r, bool on, COLORREF accent, bool darkMode)
    {
        fillAntialiasedRoundRect(dc, r, 18, on ? accent : (darkMode ? RGB(60, 70, 74) : RGB(200, 211, 206)));
        int d_inner = r.bottom - r.top - 4;
        int x = on ? r.right - d_inner - 2 : r.left + 2;
        RECT knob = layout::rect(x, r.top + 2, x + d_inner, r.bottom - 2);
        fillAntialiasedEllipse(dc, knob, darkMode ? RGB(190, 201, 197) : RGB(255, 255, 255));
    }
    void paint(HDC dc, AppState &s, svg::Renderer &renderer)
    {
        RECT client;
        GetClientRect(s.hwnd, &client);
        if (s.compact)
        {
            fillAntialiasedEllipse(dc, layout::rect(0, 0, client.right, client.bottom), surface(s));
            renderer.draw(dc, svg::Asset::Logo, layout::rect(11, 11, client.right - 11, client.bottom - 11), s.theme, true);
            return;
        }
        fillRound(dc, layout::rect(0, 0, client.right, client.bottom), surface(s), 14, kBorder);
        renderer.draw(dc, svg::Asset::Logo, layout::rect(15, 8, 49, 42), s.theme, true);
        auto title = [&](RECT r, int id)
        {
            fillRound(dc, r, s.hot == id ? (s.darkMode ? mix(surface(s), s.theme, s.pressed == id ? 20 : 30) : mix(surface(s), s.theme, s.pressed == id ? 20 : 12)) : surface(s), 6);
        };
        title(s.darkModeButton, DarkMode);
        title(s.infoButton, Info);
        title(s.settingsButton, Settings);
        title(s.minButton, Minimize);
        title(s.closeButton, Close);
        const COLORREF fixedIcon = s.darkMode ? RGB(255, 255, 255) : RGB(0, 0, 0);
        const COLORREF pageText = s.darkMode ? RGB(255, 255, 255) : kText;
        renderer.draw(dc, svg::Asset::DarkMode, layout::rect((s.darkModeButton.left + s.darkModeButton.right) / 2 - 12, (s.darkModeButton.top + s.darkModeButton.bottom) / 2 - 12, (s.darkModeButton.left + s.darkModeButton.right) / 2 + 12, (s.darkModeButton.top + s.darkModeButton.bottom) / 2 + 12), fixedIcon, true);
        renderer.draw(dc, svg::Asset::Info, iconBox(s.infoButton), fixedIcon, true);
        renderer.draw(dc, svg::Asset::Settings, iconBox(s.settingsButton), fixedIcon, true);
        renderer.draw(dc, svg::Asset::Reduce, iconBox(s.minButton), fixedIcon, true);
        renderer.draw(dc, svg::Asset::Cancel, iconBox(s.closeButton), fixedIcon, true);
        if (s.infoPage)
        {
            text(dc, L"相关信息", layout::rect(app::kMargin, 62, app::kWindowWidth - app::kMargin, 84), DT_SINGLELINE | DT_VCENTER, pageText, s.titleFont);
            text(dc, L"版本：v1.0.0", layout::rect(app::kMargin, 84, app::kWindowWidth - app::kMargin, 106), DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            text(dc, L"这是一个免费软件，如果你是付费获取的，说明你被骗了", layout::rect(app::kMargin, 108, app::kWindowWidth - app::kMargin, 156), DT_WORDBREAK | DT_LEFT, pageText, s.font);
            text(dc, L"作者：nakili", layout::rect(app::kMargin, 162, app::kWindowWidth - app::kMargin, 184), DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            renderer.draw(dc, svg::Asset::Bilibili, layout::rect(s.bilibiliLink.left + 8, s.bilibiliLink.top + 8, s.bilibiliLink.left + 32, s.bilibiliLink.top + 32), fixedIcon, true);
            renderer.draw(dc, svg::Asset::Github, layout::rect(s.githubLink.left + 8, s.githubLink.top + 8, s.githubLink.left + 32, s.githubLink.top + 32), fixedIcon, true);
            fillRound(dc, s.supportButton, s.hot == Support ? lightTheme(s.theme) : surface(s), 6, settingsBorder(s.theme));
            text(dc, L"支持净文", s.supportButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE, pageText, s.font);
            if (s.supportOpen)
            {
                text(dc, L"如果喜欢净文，可以请作者喝一杯咖啡", layout::rect(app::kMargin, 292, app::kWindowWidth - app::kMargin, 314), DT_SINGLELINE | DT_VCENTER, pageText, s.font);
                renderer.drawPayment(dc, s.paymentImage);
            }
            return;
        }
        if (s.settings)
        {
            fillRound(dc, s.settingsRect, s.darkMode ? darkSettingsTheme(s.theme) : settingsTheme(s.theme), 10, settingsBorder(s.theme));
            text(dc, L"\u5f00\u673a\u81ea\u542f\u52a8",
                 layout::rect(s.settingsRect.left + 16, s.startupCheck.top - 2, s.startupCheck.left - 14, s.startupCheck.bottom + 3),
                 DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            toggle(dc, s.startupCheck, s.startup, s.theme, s.darkMode);
            text(dc, L"\u60ac\u6d6e\u5728\u6700\u4e0a\u5c42",
                 layout::rect(s.settingsRect.left + 16, s.topmostCheck.top - 2, s.topmostCheck.left - 14, s.topmostCheck.bottom + 3),
                 DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            toggle(dc, s.topmostCheck, s.topmost, s.theme, s.darkMode);
            // 主题色按钮 - GDI+ 抗锯齿
            fillAntialiasedEllipse(dc, s.colorButton, s.theme);
            text(dc, L"\u4e3b\u9898\u989c\u8272",
                 layout::rect(s.settingsRect.left + 16, s.colorButton.top - 2, s.colorButton.left - 14, s.colorButton.bottom + 3),
                 DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            if (s.colorPicker)
            {
                static constexpr COLORREF colors[] = {RGB(45, 212, 163), RGB(66, 133, 244), RGB(154, 102, 255), RGB(245, 140, 66), RGB(235, 87, 87)};
                for (size_t i = 0; i < s.presetColors.size(); ++i)
                {
                    fillAntialiasedEllipse(dc, s.presetColors[i], colors[i]);
                }
                fillRound(dc, s.colorInputRect, surface(s), 4, settingsBorder(s.theme));
            }
            const int filterLabelTop = s.settingsRect.top + (s.colorPicker ? 166 : 104);
            text(dc, L"过滤内容", layout::rect(s.settingsRect.left + 16, filterLabelTop, s.settingsRect.right - 16, filterLabelTop + 18), DT_SINGLELINE | DT_VCENTER, pageText, s.font);
            for (size_t i = 0; i < s.filters.size(); ++i)
            {
                const auto &item = s.filters[i];
                fillRound(dc, item.button, s.darkMode ? (item.selected ? mix(surface(s), s.theme, 55) : surface(s)) : (item.selected ? lightTheme(s.theme) : RGB(255, 255, 255)), 6, settingsBorder(s.theme));
                text(dc, item.text.c_str(), layout::rect(item.button.left + 8, item.button.top, item.button.right - (item.builtin ? 8 : 16), item.button.bottom), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, pageText, s.font);
                if (!item.builtin && s.hotFilter == static_cast<int>(i))
                {
                    fillAntialiasedEllipse(dc, item.deleteButton, RGB(255, 255, 255));
                    text(dc, L"×", item.deleteButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(113, 129, 122), s.font);
                }
            }
            fillRound(dc, s.filterInputRect, surface(s), 4, settingsBorder(s.theme));
        }
        fillRound(dc, s.inputRect, surface(s), 10, settingsBorder(s.theme));
        if (GetWindowTextLengthW(s.input) > 0)
        {
            fillRound(dc, s.clearButton, s.darkMode ? surface(s) : (s.hot == ClearInput ? RGB(207, 245, 229) : RGB(255, 255, 255)), 14);
            renderer.draw(dc, svg::Asset::Cancel, s.clearButton, s.theme, true);
        }
        paintScroll(dc, s, s.input, s.inputRect);
        if (!s.result.empty())
        {
            fillRound(dc, s.outputRect, surface(s), 10, settingsBorder(s.theme));
            fillRound(dc, s.deleteButton, s.darkMode ? surface(s) : (s.hot == DeleteOutput ? RGB(207, 245, 229) : RGB(255, 255, 255)), 14);
            renderer.draw(dc, svg::Asset::Cancel, s.deleteButton, s.theme, true);
            fillRound(dc, s.copyButton, s.darkMode ? surface(s) : (s.hot == CopyOutput ? RGB(207, 245, 229) : RGB(255, 255, 255)), 14);
            renderer.draw(dc, svg::Asset::Copy, s.copyButton, s.theme, true);
            if (s.copied)
            {
                const int copyCenter = (s.copyButton.left + s.copyButton.right) / 2;
                RECT tip = layout::rect(copyCenter - 29, s.copyButton.top - 30, copyCenter + 29, s.copyButton.top - 8);
                fillAntialiasedRoundRect(dc, tip, 8, s.theme);
                text(dc, L"\u5df2\u590d\u5236", tip, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255), s.font);
            }
            paintScroll(dc, s, s.output, s.outputRect);
        }
    }
}
