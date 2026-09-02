#pragma once
#include "svg_renderer.hpp"
namespace win { HWND create(HINSTANCE, svg::Renderer&); int run(); void processInput(); void runSelfTest(); }
