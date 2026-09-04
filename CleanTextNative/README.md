# CleanText Native

Windows 10/11 x64 的原生 Win32 编译项目。使用 `nanosvg` 把 SVG 资源渲染到 GDI 位图，再叠到 Win32 控件上——整个程序没有第三方运行时依赖，单文件可执行。

## 前置条件

- Windows 10 1809+ / Windows 11
- Visual Studio 2022 或 Build Tools 2022
  - 工作负载：**使用 C++ 的桌面开发**
  - 组件：Windows 11 SDK（或 Windows 10 SDK）

> 如果同时装有多个版本（Enterprise / Professional / Community / BuildTools），脚本会自动按顺序探测并选用第一个。

## 构建

在 **仓库根目录** 或 **`CleanTextNative/`** 目录下执行：

```bat
CleanTextNative\build.bat
```

脚本会：

1. 探测 Visual Studio 2022 安装位置
2. 调用 `vcvars64.bat` 初始化 MSVC 环境
3. 调用 MSBuild 编译 `Release|x64`
4. 产物输出到 `build\x64\Release\CleanText.exe`（约 600 KB，单文件）

## 自检

编译完成后可以跑：

```bat
build\x64\Release\CleanText.exe --selftest
```

它会自动模拟几次"清空 `*`"操作，把结果写到 `cleantext_selftest.txt`（在当前工作目录下），然后退出。

## 工程目录说明

```
CleanTextNative/
├─ CleanTextNative.vcxproj    # MSBuild 工程
├─ CleanTextNative.rc          # 资源声明
├─ app.manifest                # asInvoker + PerMonitorV2 DPI
├─ src/                        # 模块化实现
│  ├─ main.cpp                 # 启动、消息循环与清理
│  ├─ app_state.hpp            # 共享 UI 状态
│  ├─ layout.* / paint.*       # 几何计算与绘制
│  ├─ svg_renderer.*           # SVG、D2D/D3D/WIC/GDI+ 后端
│  ├─ icon_theming.*           # 主题颜色解析
│  ├─ system_integration.*     # 剪贴板、启动项
│  ├─ win32_window.*           # Win32 消息与子类过程
│  └─ selftest.*               # --selftest 流程
├─ nanosvg.h                   # SVG 解析（header-only 实现）
├─ nanosvgrast.h               # SVG 光栅化（header-only 实现）
├─ resource.h                  # 资源 ID 定义
├─ resources/                  # app_*、icon_*、support_* 嵌入资源
├─ docs/architecture.md         # 模块职责与命名约定
└─ build.bat                   # 一键构建脚本
```

`resources/` 中的 SVG、应用图标和二维码会被 [`CleanTextNative.rc`](CleanTextNative.rc:1) 以资源形式嵌入；发布时只需要根目录 `CleanText.exe`。
