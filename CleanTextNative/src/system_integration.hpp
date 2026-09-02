#pragma once
#include "app_state.hpp"
namespace sys { std::wstring text(HWND); bool startupEnabled(); void setStartup(app::AppState&, bool); void copyResult(app::AppState&); }
