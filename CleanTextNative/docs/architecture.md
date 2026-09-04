# CleanText Native 架构

## 职责边界

- `core`：应用入口与共享状态。
- `ui`：窗口消息、布局、绘制、主题解析。
- `rendering`：SVG 栅格化与二维码图片绘制。
- `platform`：剪贴板、开机启动及 Win32 基础声明。
- `testing`：`--selftest` 行为回归。
- `resources`：应用图标、SVG 图标和二维码；资源文件通过 `CleanTextNative.rc` 嵌入，不依赖运行目录。

## 运行路径

`main` 初始化渲染器并创建窗口。窗口模块维护输入、设置、信息页和深色模式状态；布局模块计算所有几何区域；绘制模块按状态渲染；平台模块隔离系统调用。

## 命名约定

- C++ 文件：小写 `snake_case`。
- SVG 图标：`icon_<purpose>.svg`。
- 应用资源：`app_<purpose>.<ext>`。
- 辅助图片：`support_<purpose>.<ext>`。

## 体积说明

Release x64 当前约 608 KiB。资源区占比最大，其中 `app_icon.ico` 和 `app_logo.svg` 是主要来源。除非重新设计图标资源，否则不建议以牺牲视觉为代价压缩它们。
