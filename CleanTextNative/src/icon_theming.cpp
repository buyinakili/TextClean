#include "icon_theming.hpp"
namespace theming {
bool parseColor(const std::wstring& text, COLORREF& color) { unsigned r, g, b; if ((swscanf_s(text.c_str(), L"#%02x%02x%02x", &r, &g, &b) == 3 || swscanf_s(text.c_str(), L"%u,%u,%u", &r, &g, &b) == 3) && r < 256 && g < 256 && b < 256) { color = RGB(r, g, b); return true; } return false; }
COLORREF preset(size_t index) { static constexpr COLORREF colors[] = {RGB(45,212,163), RGB(66,133,244), RGB(154,102,255), RGB(245,140,66), RGB(235,87,87)}; return colors[index]; }
}
