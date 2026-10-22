# AE Locale Bridge

**让英文版 After Effects 正确显示中文插件文字。**

面向 **Windows x64 / 英文版 AE 2024 及以上版本**，修复中文插件的参数名、下拉选项及其他标准效果界面文字乱码。由 **MYdesignTool** 基于 **AdoreLowen / AE-UTF8-Fixer** 持续开发，新增旧版英文宿主适配、双向编码保护和更完整的界面文字处理。

[下载最新版本](https://github.com/MYdesignTool/AE-UTF8-Fixer-Ae2024/releases/latest) · [版本记录](CHANGELOG.md) · [作者与致谢](AUTHORS.md) · [反馈问题](https://github.com/MYdesignTool/AE-UTF8-Fixer-Ae2024/issues)

## 功能

- **中文界面文字修复**：处理参数名、分组标题、下拉选项、复选框、按钮和滑块说明中的 GBK / UTF-8 编码问题。
- **下拉选项问号修复**：保护中文在英文宿主编码转换过程中的文字内容，支持同一菜单混合使用 GBK 和 UTF-8 的情况。
- **更多文字入口**：覆盖标准效果界面的 Options 按钮、部分效果标题、关于信息和错误提示。
- **动态界面支持**：处理插件加载后新增或更新的标准参数文字，兼容多种效果注册方式。
- **保留原有内容**：保留已有 UTF-8、英文和菜单分隔项，不改变选项顺序、参数 ID 或参数数值。
- **独立安装**：无需更改系统区域设置，也无需修改 AE、第三方插件或工程文件。

## 兼容性

当前正式版：**0.2.2**。已在英文 **AE 24.6.2** 中由用户实测，所测文字和此前出现问号的下拉选项均正常显示。

| 环境 | 默认行为 |
| --- | --- |
| 英文 AE 24.x / 2024、25.x / 2025、26.0 / 26.1 | 启用文字归一化与旧版英文宿主编码桥接 |
| 英文 AE 26.2 及以后 | 启用文字归一化，默认关闭旧版桥接 |
| AE 2023 及更早版本 | 不启用 |
| 非英文 AE 界面 | 默认不启用 |

其他版本及插件的实际兼容性仍需验证。版本策略可通过配置调整，后续 AE 版本的兼容性以实际使用结果为准。

本项目处理使用 AE 标准效果接口传递的文字；CEP / UXP / ScriptUI 面板、自绘界面和插件独立窗口不在覆盖范围内。它不会将英文插件翻译成中文，也无法恢复原始内容已经丢失、只剩问号的文字。静态 PiPL 资源中的效果名和分类不做重写。

## 安装

1. 关闭 After Effects。
2. 从 [Releases](https://github.com/MYdesignTool/AE-UTF8-Fixer-Ae2024/releases/latest) 下载 `AE-Locale-Bridge-English-0.2.2-win64.zip` 并解压。
3. 将 `!00_AE_Locale_Bridge.aex` 和 `AE-Locale-Bridge.ini` 放入目标 AE 的 `<AE 安装目录>\Support Files\Plug-ins\`。
4. 保持插件文件名，移走同目录或其他插件目录中的旧版修复器 `!00_UTF8_Fixer.aex`，避免重复加载。升级本项目时替换旧的 `!00_AE_Locale_Bridge.aex`。
5. 启动英文版 AE，正常使用中文插件。

默认配置即可使用；配置文件缺失时也会使用默认值。安装包内的 `SHA256SUMS.txt` 可用于核对插件文件。

**卸载**：关闭 AE 后，删除 `!00_AE_Locale_Bridge.aex` 和 `AE-Locale-Bridge.ini`。

## 可选配置

编辑与插件同目录的 `AE-Locale-Bridge.ini`，保存后重启 AE。

```ini
[Bridge]
Enabled=1
OnlyEnglish=1
SourceEncoding=auto
LegacyBridge=auto
```

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `Enabled` | `1` | 设置为 `0` 停用修复 |
| `OnlyEnglish` | `1` | 只在英文 AE 界面启用；读取界面语言失败时也停用 |
| `SourceEncoding` | `auto` | 优先保留有效 UTF-8，再识别包含汉字的 GBK；`gbk` 放宽为允许无汉字的 GBK，`utf8` 关闭 GBK 转换 |
| `LegacyBridge` | `auto` | 按版本选择旧版编码桥接；`on` 强制启用，`off` 关闭 |

编码识别存在字节歧义。一般保持 `auto`；有具体兼容问题时，再结合插件情况调整。

## 问题反馈

请在 [Issues](https://github.com/MYdesignTool/AE-UTF8-Fixer-Ae2024/issues) 中附上 AE 版本、界面语言、插件名称和版本、异常位置截图及所用配置，方便定位问题。

运行日志位于 `%TEMP%\AE-Locale-Bridge-<进程ID>.log`，不记录参数原文。提供日志有助于确认插件是否已加载及编码桥接是否启用。

## 开发与贡献

本仓库由 **MYdesignTool** 维护。相较原始实现，本分支增加了英文 AE 2024+ 的版本与语言适配、中文文字双向编码保护、混合编码菜单处理、动态界面及更多 SDK 文字入口覆盖，并完善了 Hook 生命周期、回归测试与发布打包。

欢迎提交问题反馈与改进。构建说明见 [BUILDING.md](BUILDING.md)，版本变更见 [CHANGELOG.md](CHANGELOG.md)。

## 作者、来源与许可证

- **[AdoreLowen](https://github.com/AdoreLowen)**：原始 [AE-UTF8-Fixer](https://github.com/AdoreLowen/AE-UTF8-Fixer) 项目与初版核心实现作者。
- **[MYdesignTool](https://github.com/MYdesignTool)**：本仓库维护者，负责英文 AE 2024+ 适配、后续编码与界面修复、测试及发布工作。
- **Tsuda Kageyu 与 MinHook contributors**：提供 [MinHook](https://github.com/TsudaKageyu/minhook)，依赖许可证随安装包保留。

本项目采用 [MIT 许可证](LICENSE)，保留原作者版权声明与原始提交历史。完整归属及来源见 [AUTHORS.md](AUTHORS.md) 和 [UPSTREAM.md](UPSTREAM.md)。Adobe After Effects SDK 归 Adobe 所有；安装包不包含 SDK 或第三方商业插件。
