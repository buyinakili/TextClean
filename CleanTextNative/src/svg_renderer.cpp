#include "graphics_backend.hpp"
#include "svg_renderer.hpp"
#include "resource.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Msimg32.lib")

namespace svg {
struct Cache { HBITMAP bitmap{}; int width{}, height{}; COLORREF color{}; };
struct Renderer::Impl {
    ULONG_PTR gdiplusToken{}; Gdiplus::Image* payment{}; IStream* paymentStream{};
    Cache logoCache{}, cancelCache{}, copyCache{}, settingsCache{}, infoCache{}, bilibiliCache{}, githubCache{}, darkModeCache{}, reduceCache{};
};
static Cache& cacheFor(Renderer::Impl& i, Asset asset) { switch (asset) { case Asset::Logo: return i.logoCache; case Asset::Cancel: return i.cancelCache; case Asset::Copy: return i.copyCache; case Asset::Settings: return i.settingsCache; case Asset::Info: return i.infoCache; case Asset::Bilibili: return i.bilibiliCache; case Asset::Github: return i.githubCache; case Asset::DarkMode: return i.darkModeCache; default: return i.reduceCache; } }
static int resourceFor(Asset asset) { switch (asset) { case Asset::Logo: return IDR_ICON_SVG; case Asset::Cancel: return IDR_CANCEL_SVG; case Asset::Copy: return IDR_COPY_SVG; case Asset::Settings: return IDR_SETTINGS_SVG; case Asset::Info: return IDR_INFO_SVG; case Asset::Bilibili: return IDR_BILIBILI_SVG; case Asset::Github: return IDR_GITHUB_SVG; case Asset::DarkMode: return IDR_DARK_MODE_SVG; default: return IDR_REDUCE_SVG; } }
static std::string themedSvg(int id, COLORREF c) {
    HRSRC r = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id), RT_RCDATA); if (!r) return {};
    const char* p = static_cast<const char*>(LockResource(LoadResource(GetModuleHandleW(nullptr), r))); std::string s(p, SizeofResource(GetModuleHandleW(nullptr), r));
    if (id == IDR_CANCEL_SVG || id == IDR_COPY_SVG || id == IDR_SETTINGS_SVG || id == IDR_INFO_SVG || id == IDR_BILIBILI_SVG || id == IDR_GITHUB_SVG || id == IDR_DARK_MODE_SVG || id == IDR_REDUCE_SVG) { size_t path=s.find("<path"); size_t begin=path==std::string::npos?std::string::npos:s.find("d=\"",path); if (begin != std::string::npos) { begin += 3; size_t end=s.find('"', begin); std::string d=s.substr(begin, end-begin); char hex[8]; sprintf_s(hex, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c)); return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path fill=\"" + std::string(hex) + "\" d=\"" + d + "\"/></svg>"; } }
    size_t doc=s.find("<!DOCTYPE"); if (doc != std::string::npos) { size_t end=s.find('>', doc); if (end != std::string::npos) s.erase(doc, end-doc+1); }
    if (id == IDR_ICON_SVG && s.find("viewBox=") == std::string::npos && s.find("width=") == std::string::npos) { size_t tag=s.find("<svg"), end=s.find('>', tag); s.insert(end, " viewBox=\"0 0 1110 1024\""); }
    if (id == IDR_ICON_SVG) { char hex[8]; sprintf_s(hex, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c)); size_t pos=0; while ((pos=s.find("fill=\"#", pos)) != std::string::npos) { size_t end=s.find('"', pos+6); if (end == std::string::npos) break; s.replace(pos+6, end-pos-6, hex); pos=end+1; } }
    return s;
}
bool Renderer::initialize(HINSTANCE instance) {
    if (impl_) return true; impl_ = new Impl; Gdiplus::GdiplusStartupInput startup; Gdiplus::GdiplusStartup(&impl_->gdiplusToken, &startup, nullptr);
    auto load = [&](int id, IStream*& stream) -> Gdiplus::Image* { HRSRC r=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA); if(!r) return nullptr; HGLOBAL d=LoadResource(instance,r); stream=SHCreateMemStream(static_cast<const BYTE*>(LockResource(d)),SizeofResource(instance,r)); return stream ? Gdiplus::Image::FromStream(stream) : nullptr; };
    impl_->payment=load(IDR_PAYMENT_PNG,impl_->paymentStream); return true;
}
void Renderer::shutdown() {
    if (!impl_) return; for (Cache* c : {&impl_->logoCache,&impl_->cancelCache,&impl_->copyCache,&impl_->settingsCache,&impl_->infoCache,&impl_->bilibiliCache,&impl_->githubCache,&impl_->darkModeCache,&impl_->reduceCache}) if (c->bitmap) DeleteObject(c->bitmap);
    delete impl_->payment; if(impl_->paymentStream)impl_->paymentStream->Release(); Gdiplus::GdiplusShutdown(impl_->gdiplusToken); delete impl_; impl_ = nullptr;
}
void Renderer::draw(HDC out, Asset asset, RECT r, COLORREF theme, bool themed) {
    if (!impl_) return; Cache& cache=cacheFor(*impl_,asset); int w=r.right-r.left,h=r.bottom-r.top; COLORREF c=themed?theme:RGB(0,0,0); if(cache.bitmap&&(cache.width!=w||cache.height!=h||cache.color!=c)){DeleteObject(cache.bitmap);cache.bitmap=nullptr;} if(!cache.bitmap){std::string xml=themedSvg(resourceFor(asset),c); NSVGimage* image=nsvgParse(xml.data(),"px",96.0f); if(image){std::vector<unsigned char> rgba(w*h*4); NSVGrasterizer* rast=nsvgCreateRasterizer(); float scale=std::min(w/image->width,h/image->height),ox=(w-image->width*scale)/2,oy=(h-image->height*scale)/2; nsvgRasterize(rast,image,ox,oy,scale,rgba.data(),w,h,w*4); BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB; BYTE* bits{};cache.bitmap=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,(void**)&bits,nullptr,0);for(int i=0;i<w*h;i++){BYTE a=rgba[i*4+3];bits[i*4]=BYTE(rgba[i*4+2]*a/255);bits[i*4+1]=BYTE(rgba[i*4+1]*a/255);bits[i*4+2]=BYTE(rgba[i*4]*a/255);bits[i*4+3]=a;}cache.width=w;cache.height=h;cache.color=c;nsvgDeleteRasterizer(rast);nsvgDelete(image);}}
    if(cache.bitmap){HDC mem=CreateCompatibleDC(out);HGDIOBJ old=SelectObject(mem,cache.bitmap);BLENDFUNCTION b{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(out,r.left,r.top,w,h,mem,0,0,w,h,b);SelectObject(mem,old);DeleteDC(mem);}
}
void Renderer::drawPayment(HDC out, RECT r) { if (!impl_ || !impl_->payment || impl_->payment->GetLastStatus() != Gdiplus::Ok) return; Gdiplus::Graphics graphics(out); graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic); graphics.DrawImage(impl_->payment, r.left, r.top, r.right-r.left, r.bottom-r.top); }
}
