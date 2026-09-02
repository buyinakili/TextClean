#pragma once
#include "app_state.hpp"
#include "svg_renderer.hpp"
namespace ui::paint { void fillRound(HDC, RECT, COLORREF, int, COLORREF stroke = CLR_INVALID); void paint(HDC, app::AppState&, svg::Renderer&); bool hasScroll(const app::AppState&, HWND); RECT scrollTrack(const RECT&); void scrollTo(app::AppState&, HWND, const RECT&, int); }
