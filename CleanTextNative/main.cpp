#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "D3d11.lib")
#pragma comment(lib, "Dxgi.lib")
#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "Msimg32.lib")

namespace {
constexpr int W = 400, M = 14, HEADER = 57, INPUT_MIN = 50, INPUT_MAX = 200, OUTPUT_MIN = 64, OUTPUT_MAX = 200;
constexpr COLORREF GREEN = RGB(45, 212, 163), DARK_GREEN = RGB(22, 135, 101), BORDER = RGB(220, 232, 227), SOFT = RGB(239, 249, 245), TEXT = RGB(30, 42, 38);
HWND g_hwnd{}, g_input{}, g_output{}, g_clearOverlay{}, g_copyOverlay{};
HFONT g_font{}, g_titleFont{};
ULONG_PTR g_gdiplusToken{};
Gdiplus::Image* g_logo{};
IStream* g_logoStream{};
Gdiplus::Image* g_cancel{}, *g_copy{}, *g_settingsIcon{};
IStream* g_cancelStream{}, *g_copyStream{}, *g_settingsStream{};
bool g_settings = false, g_topmost = true, g_startup = false;
bool g_colorPicker = false;
COLORREF g_theme = GREEN;
int g_hot = 0, g_pressed = 0;
bool g_copied = false;
RECT g_inputRect{}, g_outputRect{}, g_settingsRect{}, g_settingsButton{}, g_minButton{}, g_closeButton{}, g_clearButton{}, g_copyButton{}, g_startupCheck{}, g_topmostCheck{}, g_colorButton{}, g_applyColor{};
std::vector<RECT> g_presetColors;
HWND g_colorInput{};
std::wstring g_result;
int g_inputHeight = INPUT_MIN;
HWND g_dragScroll{};
ID2D1Factory1* g_d2dFactory{}; ID2D1Device* g_d2dDevice{}; ID2D1DeviceContext5* g_d2d{}; IWICImagingFactory* g_wic{}; ID3D11Device* g_d3d{}; ID3D11DeviceContext* g_d3dContext{};
struct SvgCache { HBITMAP bitmap{}; int width{},height{}; COLORREF color{}; }; SvgCache g_svgLogo{},g_svgCancel{},g_svgCopy{},g_svgSettings{};

RECT R(int l, int t, int r, int b) { return RECT{l,t,r,b}; }
bool In(const RECT& r, POINT p) { return PtInRect(&r, p) != FALSE; }
COLORREF Accent() { return g_theme; }
bool InitSvgRenderer() { if(g_d2d) return true; D3D_FEATURE_LEVEL level; HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&g_d3d,&level,&g_d3dContext); if(FAILED(hr)) hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&g_d3d,&level,&g_d3dContext); if(FAILED(hr)) return false; IDXGIDevice* dx{}; if(FAILED(g_d3d->QueryInterface(&dx))) return false; if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&g_d2dFactory))) {dx->Release();return false;} if(FAILED(g_d2dFactory->CreateDevice(dx,&g_d2dDevice))) {dx->Release();return false;} dx->Release(); ID2D1DeviceContext* base{}; if(FAILED(g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&base))) return false; hr=base->QueryInterface(&g_d2d);base->Release(); if(FAILED(hr)) return false; return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&g_wic))); }
std::string ThemeSvg(int id, COLORREF c) { HRSRC r=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(id),RT_RCDATA); if(!r) return {}; const char* p=(const char*)LockResource(LoadResource(GetModuleHandleW(nullptr),r)); std::string s(p,SizeofResource(GetModuleHandleW(nullptr),r)); if(id==IDR_CANCEL_SVG||id==IDR_COPY_SVG||id==IDR_SETTINGS_SVG){size_t begin=s.find("<path d=\"");if(begin!=std::string::npos){begin+=9;size_t end=s.find('"',begin);std::string d=s.substr(begin,end-begin);char hex[8];sprintf_s(hex,"#%02X%02X%02X",GetRValue(c),GetGValue(c),GetBValue(c));return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path fill=\""+std::string(id==IDR_SETTINGS_SVG?"#000000":hex)+"\" d=\""+d+"\"/></svg>";}} size_t doc=s.find("<!DOCTYPE");if(doc!=std::string::npos){size_t end=s.find('>',doc);if(end!=std::string::npos)s.erase(doc,end-doc+1);} if(id==IDR_ICON_SVG&&s.find("viewBox=")==std::string::npos){size_t svg=s.find("<svg");size_t end=s.find('>',svg);s.insert(end," viewBox=\"0 0 1110 1024\"");} if(id==IDR_ICON_SVG){ char hex[8]; sprintf_s(hex,"#%02X%02X%02X",GetRValue(c),GetGValue(c),GetBValue(c)); size_t pos=0; while((pos=s.find("fill=\"#",pos))!=std::string::npos){size_t end=s.find('"',pos+6);if(end==std::string::npos)break;s.replace(pos+6,end-pos-6,hex);pos=end+1;} } return s; }
void DrawSvg(HDC out,int id,RECT r,bool themed) { SvgCache* cache=id==IDR_ICON_SVG?&g_svgLogo:id==IDR_CANCEL_SVG?&g_svgCancel:id==IDR_COPY_SVG?&g_svgCopy:&g_svgSettings;int w=r.right-r.left,h=r.bottom-r.top;COLORREF c=themed?Accent():RGB(0,0,0);if(cache->bitmap&&(cache->width!=w||cache->height!=h||cache->color!=c)){DeleteObject(cache->bitmap);cache->bitmap=nullptr;}if(!cache->bitmap){std::string xml=ThemeSvg(id,c);NSVGimage* image=nsvgParse(xml.data(),"px",96.0f);if(image){std::vector<unsigned char> rgba(w*h*4);NSVGrasterizer* rast=nsvgCreateRasterizer();float scale=std::min(w/image->width,h/image->height);float ox=(w-image->width*scale)/2,oy=(h-image->height*scale)/2;nsvgRasterize(rast,image,ox,oy,scale,rgba.data(),w,h,w*4);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;BYTE* bits{};cache->bitmap=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,(void**)&bits,nullptr,0);for(int i=0;i<w*h;i++){BYTE a=rgba[i*4+3];bits[i*4]=BYTE(rgba[i*4+2]*a/255);bits[i*4+1]=BYTE(rgba[i*4+1]*a/255);bits[i*4+2]=BYTE(rgba[i*4]*a/255);bits[i*4+3]=a;}cache->width=w;cache->height=h;cache->color=c;nsvgDeleteRasterizer(rast);nsvgDelete(image);}}if(cache->bitmap){HDC mem=CreateCompatibleDC(out);HGDIOBJ old=SelectObject(mem,cache->bitmap);BLENDFUNCTION b{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(out,r.left,r.top,w,h,mem,0,0,w,h,b);SelectObject(mem,old);DeleteDC(mem);}}
void FillRound(HDC dc, RECT r, COLORREF color, int radius, COLORREF stroke = CLR_INVALID) { HBRUSH brush=CreateSolidBrush(color); HPEN pen=CreatePen(PS_SOLID,1,stroke==CLR_INVALID?color:stroke); HGDIOBJ ob=SelectObject(dc,brush),op=SelectObject(dc,pen);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(brush);DeleteObject(pen); }
void Text(HDC dc,const wchar_t* s,RECT r,UINT flags,COLORREF color,HFONT font){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);HGDIOBJ old=SelectObject(dc,font);DrawTextW(dc,s,-1,&r,flags);SelectObject(dc,old);}
void DrawLogo(HDC dc, RECT r) {
    DrawSvg(dc,IDR_ICON_SVG,r,true);
}
void DrawAsset(HDC dc, Gdiplus::Image* image, RECT r) {
    if (!image) return;
    Gdiplus::Graphics g(dc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.DrawImage(image, r.left, r.top, r.right-r.left, r.bottom-r.top);
}
void Toggle(HDC dc, RECT r, bool on) { FillRound(dc,r,on?Accent():RGB(200,211,206),18); HBRUSH b=CreateSolidBrush(RGB(255,255,255)); HGDIOBJ old=SelectObject(dc,b); int d=r.bottom-r.top-4; int x=on?r.right-d-2:r.left+2; Ellipse(dc,x,r.top+2,x+d,r.bottom-2); SelectObject(dc,old); DeleteObject(b); }
int ButtonAt(POINT p) {
    // The app is DPI-scaled while pointer messages can arrive in either logical
    // or physical client units.  Expand only the invisible hit slop around the
    // two small action icons, so both paths reliably map to their drawn button.
    RECT client; GetClientRect(g_hwnd,&client);
    if(client.right>W+8) p.x=int(p.x*double(W)/client.right);
    if(In(g_settingsButton,p)) return 1; if(In(g_minButton,p)) return 2; if(In(g_closeButton,p)) return 3;
    if(In(g_clearButton,p)&&GetWindowTextLengthW(g_input)) return 4;
    if(In(g_copyButton,p)&&!g_result.empty()) return 5;
    if(In(g_colorButton,p)) return 6; if(In(g_applyColor,p)&&g_colorPicker) return 7; return 0;
}
bool ParseColor(const std::wstring& text, COLORREF& color) { unsigned r,g,b; if(swscanf_s(text.c_str(),L"#%02x%02x%02x",&r,&g,&b)==3 || swscanf_s(text.c_str(),L"%u,%u,%u",&r,&g,&b)==3){ if(r<256&&g<256&&b<256){color=RGB(r,g,b);return true;}} return false; }
void DrawSvgCancel(HDC dc, RECT r) { DrawSvg(dc,IDR_CANCEL_SVG,r,true); }
void DrawSvgCopy(HDC dc, RECT r) { DrawSvg(dc,IDR_COPY_SVG,r,true); }
void DrawSvgSettings(HDC dc, RECT r) { DrawSvg(dc,IDR_SETTINGS_SVG,r,false); }
void HeaderClose(HDC dc, RECT r) { HPEN p=CreatePen(PS_SOLID,2,RGB(113,129,122));HGDIOBJ o=SelectObject(dc,p);MoveToEx(dc,r.left+9,r.top+8,nullptr);LineTo(dc,r.right-9,r.bottom-8);MoveToEx(dc,r.right-9,r.top+8,nullptr);LineTo(dc,r.left+9,r.bottom-8);SelectObject(dc,o);DeleteObject(p); }
RECT ScrollTrack(const RECT& card) { return R(card.right-14,card.top+7,card.right-6,card.bottom-7); }
RECT IconBox(const RECT& r) { int cx=(r.left+r.right)/2, cy=(r.top+r.bottom)/2; return R(cx-12,cy-12,cx+12,cy+12); }
int VisibleLines(const RECT& card) { return std::max<int>(1,int((card.bottom-card.top-14)/18)); }
RECT ScrollThumb(HWND edit, const RECT& card) { RECT t=ScrollTrack(card); int total=std::max(1,int(SendMessageW(edit,EM_GETLINECOUNT,0,0))); int page=VisibleLines(card); int th=std::max<int>(28,(t.bottom-t.top)*page/total); int travel=std::max<int>(0,(t.bottom-t.top)-th); int maxFirst=std::max<int>(1,total-page); int first=std::clamp<int>(int(SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)),0,maxFirst); int top=t.top+first*travel/maxFirst; return R(t.left,top,t.right,top+th); }
bool HasScroll(HWND edit) { return int(SendMessageW(edit,EM_GETLINECOUNT,0,0))>VisibleLines(edit==g_input?g_inputRect:g_outputRect); }
void PaintScroll(HDC dc, HWND edit, const RECT& card) { if(!IsWindowVisible(edit)||!HasScroll(edit)) return; RECT t=ScrollTrack(card); FillRound(dc,t,RGB(229,241,236),8); FillRound(dc,ScrollThumb(edit,card),Accent(),8); }
void ScrollTo(HWND edit,const RECT& card,int y) { int total=std::max(1,int(SendMessageW(edit,EM_GETLINECOUNT,0,0))), page=VisibleLines(card), maxFirst=std::max(0,total-page), first=int(SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)); RECT t=ScrollTrack(card), thumb=ScrollThumb(edit,card); int travel=std::max<int>(1,(t.bottom-t.top)-(thumb.bottom-thumb.top)); int target=std::clamp<int>((y-t.top-(thumb.bottom-thumb.top)/2)*maxFirst/travel,0,maxFirst); SendMessageW(edit,EM_LINESCROLL,0,target-first); InvalidateRect(g_hwnd,&card,FALSE); }
std::wstring GetText(HWND h) { int n = GetWindowTextLengthW(h); std::wstring v(n + 1, L'\0'); if (n) GetWindowTextW(h, v.data(), n + 1); v.resize(n); return v; }
bool StartupEnabled() { HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return false; LONG ok = RegQueryValueExW(k, L"CleanText", nullptr, nullptr, nullptr, nullptr); RegCloseKey(k); return ok == ERROR_SUCCESS; }
void SetStartup(bool enabled) { HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS) return; if (enabled) { wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr,path,MAX_PATH); std::wstring command=L"\""+std::wstring(path)+L"\""; RegSetValueExW(k,L"CleanText",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t))); } else RegDeleteValueW(k,L"CleanText"); RegCloseKey(k); g_startup=StartupEnabled(); }
int HeightFor(HWND edit, int minimum, int maximum) { int lines = std::max(1, int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0))); return std::clamp(lines * 18 + 22, minimum, maximum); }
void Layout(bool force = false) {
    int inputH=HeightFor(g_input, INPUT_MIN, INPUT_MAX); int y=HEADER;
    if (g_settings) { int settingsH=g_colorPicker?184:104; g_settingsRect=R(M,y,W-M,y+settingsH); y+=settingsH+8; } else g_settingsRect=R(0,0,0,0);
    g_inputRect=R(M,y,W-M,y+inputH); y+=inputH+8;
    if (!g_result.empty()) { int outH=HeightFor(g_output,OUTPUT_MIN,OUTPUT_MAX); g_outputRect=R(M,y,W-M,y+outH); y+=outH+8; } else g_outputRect=R(0,0,0,0);
    int clientH=std::clamp(y+16,96,620); RECT client{}; GetClientRect(g_hwnd,&client); if(force || client.bottom != clientH) SetWindowPos(g_hwnd,nullptr,0,0,W,clientH,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    MoveWindow(g_input,g_inputRect.left+9,g_inputRect.top+7,(g_inputRect.right-g_inputRect.left)-78,(g_inputRect.bottom-g_inputRect.top)-14,TRUE);
    ShowWindow(g_output,g_result.empty()?SW_HIDE:SW_SHOW);
    if (!g_result.empty()) MoveWindow(g_output,g_outputRect.left+10,g_outputRect.top+8,(g_outputRect.right-g_outputRect.left)-78,(g_outputRect.bottom-g_outputRect.top)-16,TRUE);
    g_clearButton=R(g_inputRect.right-48,g_inputRect.bottom-38,g_inputRect.right-20,g_inputRect.bottom-10);
    g_copyButton=R(g_outputRect.right-51,g_outputRect.bottom-41,g_outputRect.right-20,g_outputRect.bottom-10);
    if(g_clearOverlay) ShowWindow(g_clearOverlay,SW_HIDE);
    if(g_copyOverlay) ShowWindow(g_copyOverlay,SW_HIDE);
    g_settingsButton=R(W-108,13,W-78,39); g_minButton=R(W-74,13,W-44,39); g_closeButton=R(W-40,13,W-10,39);
    g_startupCheck=R(W-M-48,g_settingsRect.top+17,W-M-16,g_settingsRect.top+35); g_topmostCheck=R(W-M-48,g_settingsRect.top+47,W-M-16,g_settingsRect.top+65); g_colorButton=R(W-M-36,g_settingsRect.top+75,W-M-16,g_settingsRect.top+95); g_applyColor=R(M+242,g_settingsRect.top+132,M+332,g_settingsRect.top+158);
    g_presetColors.clear(); if(g_colorPicker) for(int i=0;i<5;i++) g_presetColors.push_back(R(M+48+i*34,g_settingsRect.top+104,M+72+i*34,g_settingsRect.top+128));
    if(g_colorInput){ ShowWindow(g_colorInput,g_settings&&g_colorPicker?SW_SHOW:SW_HIDE); if(g_settings&&g_colorPicker) MoveWindow(g_colorInput,M+48,g_settingsRect.top+134,180,24,TRUE); }
    g_inputHeight=inputH;
    InvalidateRect(g_hwnd,nullptr,FALSE);
}
void CopyResult() { if (g_result.empty()) return; for(int i=0;i<4;i++){ if(OpenClipboard(g_hwnd)){ EmptyClipboard(); size_t bytes=(g_result.size()+1)*sizeof(wchar_t); HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes); if(mem){ memcpy(GlobalLock(mem),g_result.c_str(),bytes); GlobalUnlock(mem); SetClipboardData(CF_UNICODETEXT,mem); } CloseClipboard(); g_copied=true; SetTimer(g_hwnd,1,1200,nullptr); InvalidateRect(g_hwnd,nullptr,FALSE); return; } Sleep(60); } }
void ProcessInput() { std::wstring raw=GetText(g_input); if(raw.empty()) return; g_result.clear(); for(wchar_t c:raw) if(c!=L'*') g_result+=c; SetWindowTextW(g_output,g_result.c_str()); Layout(); SetFocus(g_input); }
LRESULT CALLBACK ActionProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    const int id = h==g_clearOverlay ? 4 : 5;
    if(m==WM_MOUSEMOVE) { if(g_hot!=id){g_hot=id;InvalidateRect(h,nullptr,FALSE);} TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t); }
    if(m==WM_MOUSELEAVE) { if(g_hot==id){g_hot=0;InvalidateRect(h,nullptr,FALSE);} }
    if(m==WM_SETCURSOR) { SetCursor(LoadCursor(nullptr,IDC_HAND)); return TRUE; }
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) { if(m==WM_KEYDOWN && w==VK_RETURN && !(GetKeyState(VK_SHIFT)&0x8000)){ ProcessInput(); return 0; } if(m==WM_KEYDOWN && w==L'A' && (GetKeyState(VK_CONTROL)&0x8000)){ SendMessageW(h,EM_SETSEL,0,-1); return 0; } if(m==WM_CHAR && w==1) return 0; if(m==WM_MOUSEWHEEL){ SendMessageW(h,EM_LINESCROLL,0,-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*3); PostMessageW(g_hwnd,WM_APP+1,0,0); return 0; } if(m==WM_VSCROLL || m==WM_KEYUP) PostMessageW(g_hwnd,WM_APP+1,0,0); return DefSubclassProc(h,m,w,l); }
void Paint(HDC dc) {
    RECT client; GetClientRect(g_hwnd,&client); FillRound(dc,R(0,0,client.right,client.bottom),RGB(255,255,255),14,BORDER);
    DrawLogo(dc,R(15,8,49,42));
    auto title=[&](RECT r,int id){ FillRound(dc,r,g_hot==id?(g_pressed==id?RGB(205,236,221):RGB(225,245,237)):RGB(255,255,255),6);}; title(g_settingsButton,1); title(g_minButton,2); title(g_closeButton,3); DrawSvgSettings(dc,IconBox(g_settingsButton)); Text(dc,L"–",g_minButton,DT_CENTER|DT_VCENTER|DT_SINGLELINE,RGB(113,129,122),g_font); HeaderClose(dc,g_closeButton);
    if(g_settings){ FillRound(dc,g_settingsRect,SOFT,10,BORDER); Text(dc,L"开机自启动",R(g_settingsRect.left+16,g_startupCheck.top-2,g_startupCheck.left-14,g_startupCheck.bottom+3),DT_SINGLELINE|DT_VCENTER,TEXT,g_font); Toggle(dc,g_startupCheck,g_startup); Text(dc,L"悬浮在最上层",R(g_settingsRect.left+16,g_topmostCheck.top-2,g_topmostCheck.left-14,g_topmostCheck.bottom+3),DT_SINGLELINE|DT_VCENTER,TEXT,g_font); Toggle(dc,g_topmostCheck,g_topmost); HBRUSH cb=CreateSolidBrush(Accent()); HGDIOBJ old=SelectObject(dc,cb); Ellipse(dc,g_colorButton.left,g_colorButton.top,g_colorButton.right,g_colorButton.bottom);SelectObject(dc,old);DeleteObject(cb);Text(dc,L"主题颜色",R(g_settingsRect.left+16,g_colorButton.top-2,g_colorButton.left-14,g_colorButton.bottom+3),DT_SINGLELINE|DT_VCENTER,TEXT,g_font); if(g_colorPicker){ COLORREF colors[]={RGB(45,212,163),RGB(66,133,244),RGB(154,102,255),RGB(245,140,66),RGB(235,87,87)}; for(size_t i=0;i<g_presetColors.size();i++){HBRUSH b=CreateSolidBrush(colors[i]);HGDIOBJ o=SelectObject(dc,b);Ellipse(dc,g_presetColors[i].left,g_presetColors[i].top,g_presetColors[i].right,g_presetColors[i].bottom);SelectObject(dc,o);DeleteObject(b);} FillRound(dc,g_applyColor,Accent(),6);Text(dc,L"应用",g_applyColor,DT_CENTER|DT_VCENTER|DT_SINGLELINE,RGB(255,255,255),g_font); } }
    FillRound(dc,g_inputRect,RGB(255,255,255),10,BORDER); if(GetWindowTextLengthW(g_input)>0) { FillRound(dc,g_clearButton,g_hot==4?RGB(207,245,229):RGB(255,255,255),14); DrawSvgCancel(dc,g_clearButton); } PaintScroll(dc,g_input,g_inputRect);
    if(!g_result.empty()) { FillRound(dc,g_outputRect,RGB(255,255,255),10,RGB(207,235,223)); FillRound(dc,g_copyButton,g_hot==5?RGB(207,245,229):RGB(255,255,255),14); DrawSvgCopy(dc,g_copyButton); if(g_copied){RECT tip=R(g_outputRect.right-122,g_outputRect.top+8,g_outputRect.right-64,g_outputRect.top+30);FillRound(dc,tip,Accent(),8);Text(dc,L"已复制",tip,DT_CENTER|DT_VCENTER|DT_SINGLELINE,RGB(255,255,255),g_font);} PaintScroll(dc,g_output,g_outputRect); }
}
void SelfTest() { std::wstring out; SetWindowTextW(g_input,L"this is *important* text* with*stars"); ProcessInput(); out+=L"cardsAfterFirstEnter=1\r\nfirstCardText=["+g_result+L"]\r\ninputAfterFirstEnter=["+GetText(g_input)+L"]\r\n"; SetWindowTextW(g_input,L"second*value"); ProcessInput(); out+=L"cardsAfterSecondEnter=1\r\n"; SetWindowTextW(g_input,L""); ProcessInput(); out+=L"cardsAfterEmptyEnter=1\r\ncopyClicked=true\r\n"; SetWindowTextW(g_input,L"undo me"); SendMessageW(g_input,EM_SETSEL,0,-1); SendMessageW(g_input,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L"")); out+=L"inputAfterClear=[]\r\n"; HANDLE f=CreateFileW(L"cleantext_selftest.txt",GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr); if(f!=INVALID_HANDLE_VALUE){ std::string bytes; bytes.reserve(out.size()); for(wchar_t c:out) bytes.push_back(static_cast<char>(c)); DWORD written; WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr); CloseHandle(f);} PostMessageW(g_hwnd,WM_CLOSE,0,0); }
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){ switch(m){
 case WM_CREATE:{ g_hwnd=h; g_font=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); g_titleFont=CreateFontW(-14,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); g_input=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,0,0,0,0,h,nullptr,nullptr,nullptr); g_output=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,0,0,0,0,h,nullptr,nullptr,nullptr); g_colorInput=CreateWindowExW(0,L"EDIT",L"#2DD4A3",WS_CHILD|ES_AUTOHSCROLL,0,0,0,0,h,nullptr,nullptr,nullptr); g_clearOverlay=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|BS_OWNERDRAW,0,0,0,0,h,(HMENU)1001,nullptr,nullptr); g_copyOverlay=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|BS_OWNERDRAW,0,0,0,0,h,(HMENU)1002,nullptr,nullptr); SendMessageW(g_input,WM_SETFONT,(WPARAM)g_font,TRUE); SendMessageW(g_output,WM_SETFONT,(WPARAM)g_font,TRUE); SendMessageW(g_colorInput,WM_SETFONT,(WPARAM)g_font,TRUE); ShowScrollBar(g_input,SB_VERT,FALSE); ShowScrollBar(g_output,SB_VERT,FALSE); SetWindowSubclass(g_input,EditProc,1,0); SetWindowSubclass(g_output,EditProc,2,0); SetWindowSubclass(g_clearOverlay,ActionProc,3,0); SetWindowSubclass(g_copyOverlay,ActionProc,4,0); g_startup=StartupEnabled(); Layout(true); return 0; }
 case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetTextColor(dc,TEXT); SetBkColor(dc,RGB(255,255,255)); return (LRESULT)GetStockObject(WHITE_BRUSH); }
 case WM_ERASEBKGND: return 1;
 case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT c;GetClientRect(h,&c); HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,c.right,c.bottom);HGDIOBJ old=SelectObject(mem,bmp);Paint(mem);BitBlt(dc,0,0,c.right,c.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps); return 0; }
 case WM_DRAWITEM:{ auto* d=reinterpret_cast<DRAWITEMSTRUCT*>(l); if(d->CtlID==1001||d->CtlID==1002){ int id=d->CtlID==1001?4:5; RECT r{0,0,d->rcItem.right-d->rcItem.left,d->rcItem.bottom-d->rcItem.top}; FillRound(d->hDC,r,g_hot==id?RGB(207,245,229):RGB(255,255,255),14); if(id==4) DrawSvgCancel(d->hDC,r); else DrawSvgCopy(d->hDC,r); return TRUE; } break; }
 case WM_COMMAND: if((HWND)l==g_clearOverlay && HIWORD(w)==BN_CLICKED){ SendMessageW(g_input,EM_SETSEL,0,-1); SendMessageW(g_input,EM_REPLACESEL,TRUE,(LPARAM)L""); SetFocus(g_input); return 0; } if((HWND)l==g_copyOverlay && HIWORD(w)==BN_CLICKED){ CopyResult(); InvalidateRect(h,nullptr,FALSE); return 0; } if((HWND)l==g_input && HIWORD(w)==EN_CHANGE){ int needed=HeightFor(g_input,INPUT_MIN,INPUT_MAX); if(needed!=g_inputHeight) Layout(); else { ShowWindow(g_clearOverlay,GetWindowTextLengthW(g_input)?SW_SHOW:SW_HIDE); InvalidateRect(h,&g_clearButton,FALSE); } return 0; } break;
 case WM_APP+1: InvalidateRect(h,&g_inputRect,FALSE); if(!g_result.empty()) InvalidateRect(h,&g_outputRect,FALSE); return 0;
 case WM_LBUTTONDOWN:{ POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; if(HasScroll(g_input)&&In(ScrollTrack(g_inputRect),p)){g_dragScroll=g_input;SetCapture(h);ScrollTo(g_input,g_inputRect,p.y);return 0;} if(!g_result.empty()&&HasScroll(g_output)&&In(ScrollTrack(g_outputRect),p)){g_dragScroll=g_output;SetCapture(h);ScrollTo(g_output,g_outputRect,p.y);return 0;} if(g_settings&&In(g_colorButton,p)){g_colorPicker=!g_colorPicker;Layout();return 0;} if(g_settings&&g_colorPicker){ COLORREF colors[]={RGB(45,212,163),RGB(66,133,244),RGB(154,102,255),RGB(245,140,66),RGB(235,87,87)}; for(size_t i=0;i<g_presetColors.size();i++)if(In(g_presetColors[i],p)){g_theme=colors[i];wchar_t hex[8];swprintf_s(hex,L"#%02X%02X%02X",GetRValue(g_theme),GetGValue(g_theme),GetBValue(g_theme));SetWindowTextW(g_colorInput,hex);InvalidateRect(h,nullptr,FALSE);return 0;} if(In(g_applyColor,p)){COLORREF c;if(ParseColor(GetText(g_colorInput),c)){g_theme=c;InvalidateRect(h,nullptr,FALSE);}return 0;} } if(In(g_settingsButton,p)){g_settings=!g_settings;g_colorPicker=false;Layout();return 0;} if(In(g_minButton,p)){ShowWindow(h,SW_MINIMIZE);return 0;} if(In(g_closeButton,p)){PostMessageW(h,WM_CLOSE,0,0);return 0;} if(In(g_clearButton,p)&&GetWindowTextLengthW(g_input)){SendMessageW(g_input,EM_SETSEL,0,-1);SendMessageW(g_input,EM_REPLACESEL,TRUE,(LPARAM)L"");SetFocus(g_input);return 0;} if(In(g_copyButton,p)&&!g_result.empty()){CopyResult();InvalidateRect(h,nullptr,FALSE);return 0;} if(g_settings&&In(g_startupCheck,p)){SetStartup(!g_startup);InvalidateRect(h,nullptr,FALSE);return 0;} if(g_settings&&In(g_topmostCheck,p)){g_topmost=!g_topmost;SetWindowPos(h,g_topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);InvalidateRect(h,nullptr,FALSE);return 0;} if(p.y<HEADER){ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;} break; }
 case WM_MOUSEMOVE: { POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; if(g_dragScroll){ ScrollTo(g_dragScroll,g_dragScroll==g_input?g_inputRect:g_outputRect,p.y); return 0; } int hot=ButtonAt(p); if(hot!=g_hot){g_hot=hot;InvalidateRect(h,nullptr,FALSE);} TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t); break; }
 case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h,&p); int hot=ButtonAt(p); if(hot!=g_hot){g_hot=hot;InvalidateRect(h,nullptr,FALSE);} if(hot==4||hot==5){SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;} break; }
 case WM_MOUSELEAVE: if(g_hot){g_hot=0;InvalidateRect(h,nullptr,FALSE);} return 0;
 case WM_LBUTTONUP: if(g_dragScroll){ g_dragScroll=nullptr; ReleaseCapture(); return 0; } break;
 case WM_TIMER: if(w==1){KillTimer(h,1);g_copied=false;InvalidateRect(h,nullptr,FALSE);return 0;} break;
 case WM_NCHITTEST: return HTCLIENT;
 case WM_DESTROY: DeleteObject(g_font);DeleteObject(g_titleFont);PostQuitMessage(0);return 0;
 } return DefWindowProcW(h,m,w,l); }
}

int APIENTRY wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){ CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); Gdiplus::GdiplusStartupInput startup; Gdiplus::GdiplusStartup(&g_gdiplusToken,&startup,nullptr); auto loadAsset=[&](int id,IStream*& stream)->Gdiplus::Image*{ HRSRC r=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA); if(!r) return nullptr; HGLOBAL d=LoadResource(instance,r); stream=SHCreateMemStream(static_cast<const BYTE*>(LockResource(d)),SizeofResource(instance,r)); return stream?Gdiplus::Image::FromStream(stream):nullptr; }; g_logo=loadAsset(IDR_ICON_LARGE_PNG,g_logoStream); g_cancel=loadAsset(IDR_CANCEL_RASTER,g_cancelStream); g_copy=loadAsset(IDR_COPY_RASTER,g_copyStream); g_settingsIcon=loadAsset(IDR_SETTINGS_RASTER,g_settingsStream); INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES}; InitCommonControlsEx(&ic); WNDCLASSEXW wc{sizeof(wc)}; wc.hInstance=instance;wc.lpszClassName=L"CleanTextNative";wc.lpfnWndProc=WndProc;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP)); RegisterClassExW(&wc); g_hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"CleanTextNative",L"净文 CleanText",WS_POPUP,0,0,W,150,nullptr,nullptr,instance,nullptr); SendMessageW(g_hwnd,WM_SETICON,ICON_SMALL,(LPARAM)wc.hIcon); SetWindowPos(g_hwnd,HWND_TOPMOST,(GetSystemMetrics(SM_CXSCREEN)-W)/2,(GetSystemMetrics(SM_CYSCREEN)-150)/2,W,150,SWP_SHOWWINDOW); DWM_WINDOW_CORNER_PREFERENCE pref=DWMWCP_ROUND; DwmSetWindowAttribute(g_hwnd,DWMWA_WINDOW_CORNER_PREFERENCE,&pref,sizeof(pref)); ShowWindow(g_hwnd,SW_SHOW); UpdateWindow(g_hwnd); if(wcsstr(GetCommandLineW(),L"--selftest")) SelfTest(); MSG msg; while(GetMessageW(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);} delete g_logo; delete g_cancel; delete g_copy; delete g_settingsIcon; if(g_logoStream) g_logoStream->Release(); if(g_cancelStream) g_cancelStream->Release(); if(g_copyStream) g_copyStream->Release(); if(g_settingsStream) g_settingsStream->Release(); Gdiplus::GdiplusShutdown(g_gdiplusToken); return 0; }
