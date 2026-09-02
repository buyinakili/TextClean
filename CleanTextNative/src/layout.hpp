#pragma once
#include "app_state.hpp"
namespace ui::layout { RECT rect(int, int, int, int); bool contains(const RECT&, POINT); int heightFor(HWND, int, int); void compute(app::AppState&, bool force = false); }
