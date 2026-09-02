# CleanText Native

Windows 10/11 x64 的原生 Win32 发布项目。需要安装 Visual Studio 2022 Build Tools 的 C++ 工作负载后，在开发者命令提示符中执行：

```bat
msbuild CleanTextNative.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

生成文件为 `build\\x64\\Release\\CleanText.exe`。该项目使用静态 MSVC 运行库和 Windows 系统 DLL，不包含 .NET 运行时。
