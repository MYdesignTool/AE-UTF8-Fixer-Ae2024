# AE Locale Bridge — 英文版 AE 2024+ 候选适配版

原始项目与初版核心实现由 **[AdoreLowen](https://github.com/AdoreLowen/AE-UTF8-Fixer)** 编写，采用 MIT 许可证。本仓库是其 fork，在保留原作者版权声明、提交历史和来源说明的基础上，新增英文版 AE 24.0（2024）及以后版本的 Windows x64 适配。作者与依赖归属见 [AUTHORS.md](AUTHORS.md)。

**当前状态：0.2.2 候选版。用户反馈 0.2.0 在英文 AE 24.6.2 中大部分功能可用，但部分下拉项显示问号。已修复模拟宿主中复现的菜单 ANSI 回转丢字，并扩展检查混合编码菜单、其他 SDK 文本入口和 Hook 失败路径；9 组本地测试通过。截图中的汉化版插件仍待用户复测，不能据此宣称所有插件已兼容。**

0.2.1 针对已登记的中文显示文本，补充 Unicode → ANSI 回转保护：仅在请求的旧代码页会丢字时，保留为 UTF-8，再由解码桥接读取。原有英文项、分隔项 `(-`、菜单顺序和选项数不作替换；能无损表示中文的旧编码继续按原编码输出。完整记录见 [CHANGELOG.md](CHANGELOG.md)。

0.2.2 进一步修复同菜单混合 GBK/UTF-8 项的转换、Options 按钮、效果标题及关于/错误提示漏处理，并在 Hook 启用失败时恢复有效原函数地址，避免后续调用已释放的跳板。混合编码仍有字节歧义：存在明确 UTF-8 多字节项时逐项处理；否则优先保留既有整串 GBK 转换行为。

## 安装与撤销

1. 关闭 AE。
2. 从生成的安装包取出 `!00_AE_Locale_Bridge.aex` 和 `AE-Locale-Bridge.ini`，一起放到目标版本的：
   `<AE 安装目录>\Support Files\Plug-ins\`
3. 保持文件名，确保它尽早加载。移走原版 `!00_UTF8_Fixer.aex`，避免两个修复插件同时 Hook 相同接口。
4. 启动英文版 AE。配置文件可选，不放置时使用默认值。卸载时关闭 AE，删除这两个文件即可。

安装包不含 `FakeModern.aex`、`FakeLegacy.aex` 或任何测试程序。不要把构建目录中的测试插件一起装入 AE。

本插件不修改 AE 安装文件、第三方插件二进制、项目数据或系统区域设置。Hook 及中文字符串缓存只存在于当前进程；为了保持回调和字符串指针有效，已经 Hook 的模块会驻留至进程结束。

## 覆盖范围

- SDK 参数名、分组标题、下拉菜单、复选框文字、按钮文字、滑块文字说明。
- 标准 Effect UI Suite 的 Options 按钮名；经过可识别效果入口的序列初始化标题、关于文字及带 DISPLAY_ERROR_MESSAGE 标志的错误提示。
- `add_param`、`PF_UpdateParamUI` 及 UI 更新回调后的参数名修复。
- 老式 PiPL 声明的自定义效果入口，以及 `PluginDataEntryFunction` / `PluginDataEntryFunction2` 声明的入口。
- 已加载模块和延后加载模块。发现宿主的 `add_param` 后，同用该回调的其他插件也会经过修复，包括跳过注册、直接使用缓存入口的效果。
- 现代注册接口中的显示名和分类；不改 match name、参数 ID、入口名、URL 或参数数值。

不覆盖 CEP/UXP/ScriptUI 面板、自绘 UI、插件独立窗口、已经保存成问号的文字，以及绕过标准接口的字符串。老式 PiPL 中静态的效果名称和分类不做资源重写。插件缓存若跳过注册，现代注册接口显示名的修复也可能不会执行。

## 编码策略与版本

原项目只将非 UTF-8 字符串从 GBK 转成 UTF-8。本版先严格验证 Unicode：保留已有 UTF-8（包括西文重音和 emoji）；否则仅在 GBK 严格解码、无损回转且含汉字时转换。裸字节不能总是区分 GBK 和其他 ANSI 编码；`auto` 是启发式识别，不能恢复丢失的原始文本。

| 宿主 | 默认路径 | 验证状态 |
| --- | --- | --- |
| AE 24.x / 2024 | UTF-8 归一化 + 旧版英文解码桥接 | 24.6.2 模拟宿主通过；24.0 策略测试通过 |
| AE 25.x / 2025 | 同上 | 版本策略测试通过，真实宿主待测 |
| AE 26.0 / 26.1 | 同上 | 版本策略测试通过，真实宿主待测 |
| AE 26.2 及以后 | UTF-8 归一化，默认关闭旧版桥接 | 26.2 模拟宿主通过；后续版本待测 |
| AE < 24 或版本读取失败 | 不安装 Hook | 23.6 模拟宿主通过 |
| 非英文界面 | 默认不安装 Hook | zh_CN 模拟宿主通过 |

**旧版桥接的工作假设：**旧版宿主经 Windows `MultiByteToWideChar` 解码这些参数时，在匹配已登记的中文显示文本后按 UTF-8 解码。0.2.1 同时保护 `WideCharToMultiByte` 的有损反向转换，避免 Unicode 中文在菜单处理中提前变成 `?`。[Windows 文档](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte) 说明了非 Unicode 目标代码页的丢字风险。桥接只匹配已登记的显示文本及中文下拉项，不全局更换代码页；明确要求 UTF-8 的调用、未登记文字和无损旧编码转换照常转发。若实际 AE 构建使用内部转换器、预先丢失原文或不经过这些 Windows API，仍可能需要其他适配。

SDK 本地化说明指出旧接口的 `char` 字符串与应用 locale 有关。Adobe 的 [26.2 修复说明](https://helpx.adobe.com/after-effects/desktop/troubleshooting/fixed-and-known-issues/fixed-issues.html) 提到了第三方插件访问效果名称的 Unicode 编码修复，但它不构成所有参数接口切换 UTF-8 的保证。26.2 分界沿用原项目的适配假设，可通过配置覆盖。

## 配置与诊断

在同目录的 `AE-Locale-Bridge.ini` 中设置，修改后重启 AE：

| 键 | 默认 | 含义 |
| --- | --- | --- |
| Enabled | 1 | 0 停用 |
| OnlyEnglish | 1 | 1 只在英文 UI 启用；读取语言失败也停用 |
| SourceEncoding | auto | auto 按上述规则识别；gbk 允许无汉字的 GBK；utf8 不转换 GBK。已有有效 UTF-8 在三种模式下都保留 |
| LegacyBridge | auto | auto 按版本策略；on 强制桥接；off 关闭桥接 |

使用实际 AE 可执行文件的版本资源判断产品版本，不把 AEGP 入口的 ABI 版本当成 AE 年份。

日志在 `%TEMP%\AE-Locale-Bridge-<进程ID>.log`，也通过 OutputDebugString 输出。记录宿主版本、系统代码页、UI 语言、初始化状态及正常退出时的转换/Hook 计数，不记录用户的参数文本。`decoder_hits` / `encoder_hits` 分别记录两条桥接路径的命中次数；`encoder_buffer_failures` 记录回转时缓冲区不足，函数会返回错误，不越界写入。若菜单在进入接口时已包含纯问号项，会即时记录 `Popup input already has placeholder-only items`；这种情况下不会猜测或伪造原文。计数为零不等于显示正确。

## 编译和测试

要求 Windows x64、Visual Studio C++ Build Tools、Windows SDK、CMake 3.20+。本工作区默认使用 `../May2023_AfterEffectsSDK/Examples`；独立仓库中回退到原仓库的 `AE_SDK`，也可以显式指定包含 Headers/Resources/Util 的目录。

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON -DAE_SDK_PATH="../May2023_AfterEffectsSDK/Examples"
cmake --build build --config Release -- /m:1 /nr:false
ctest --test-dir build -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File scripts/package.ps1
```

MinHook 固定在 v1.3.4 对应的提交 `c3fcafdc10146beb5919319d0683e44e3c30d537`，本工作区已放置源码到 `third_party/minhook`，可离线构建。若该目录缺失，CMake 会获取相同提交。输出插件使用静态 MSVC 运行库。

沙箱若报告 PiPLtool 无法运行或系统 DLL 重定位错误，可在正常的 VS 开发者终端中构建；本次使用你提供的 SDK 工具在沙箱外成功生成了资源。

验证记录见 [VALIDATION.md](VALIDATION.md)。模拟宿主不是 Adobe AE；“未来版本可加载”的设计目标不等同于已经通过未来版本兼容性测试。
