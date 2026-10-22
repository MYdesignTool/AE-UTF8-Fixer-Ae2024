# 构建

需要 Windows x64、Visual Studio C++ Build Tools、Windows SDK 和 CMake 3.20+。

`AE_SDK_PATH` 应指向包含 `Headers`、`Resources` 和 `Util` 的 SDK 目录。以下示例使用仓库内的 `AE_SDK`，也可改为本地 SDK 的绝对路径。

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON -DAE_SDK_PATH="AE_SDK"
cmake --build build --config Release -- /m:1 /nr:false
ctest --test-dir build -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File scripts/package.ps1
```

插件输出为 `build/Release/!00_AE_Locale_Bridge.aex`，安装包输出到 `dist/`。构建目录内的模拟插件和测试程序仅用于测试，不应安装到 AE。

MinHook 固定在 v1.3.4 对应提交 `c3fcafdc10146beb5919319d0683e44e3c30d537`。仓库内 `third_party/minhook` 包含所需源码；目录缺失时 CMake 会获取相同提交。插件使用静态 MSVC 运行库。
