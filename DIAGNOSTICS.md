# Missing 显示诊断

0.2.4-rc.2 是诊断测试版。它修正已确认的工程读取覆盖缺口，但尚未确认能解决用户实际 AE 中的 Missing 乱码。稳定版仍为 0.2.3。

1. 关闭 AE，将测试包中的 `.aex` 与 `.ini` 一同替换到测试用 AE。
2. 保持 `ProjectTextBridge=1`、`ProjectDiagnostics=1`；使用保留的旧 INI 时请手动加入这两项。
3. 启动 AE，打开原来的 Missing 工程并查看异常文字，然后正常退出 AE。
4. 在 `%TEMP%` 中找到对应运行的 `AE-Locale-Bridge-<进程ID>.log`，核对首行为 `0.2.4-rc.2`，提供该份日志。

日志会记录插件版本、宿主版本、界面语言、工程打开接口、扫描阶段、登记字段数及命中的转换代码页。它不扫描安装目录，不记录工程路径、参数原文或菜单内容，也不会修改工程文件。

## 如何判断阶段

- `Project file hooks`：三个文件打开接口是否成功安装。
- `Project open`：是否观察到工程打开，以及该次打开是否可读取。
- `Project scan; stage=registered`：已成功解析工程缓存；`fields` 是缓存字段数，`registered` 是登记的中文字段数。
- 其他 `Project scan` 阶段：文件大小、映射、快照或缓存布局未通过检查。
- `Project decoder observed`：观察到已登记缓存文字进入 Windows 解码接口，仅输出代码页、标志与长度。
- `Project mojibake encoder observed`：观察到已登记文字的西文乱码形式进入编码接口；这是定位记录，不会额外改写这些 Unicode 字符串。
- `Shutdown`：本次运行的累计计数。

一份日志中没有 `Project open`，与出现 `Project open` 但扫描失败，是两种不同情况；仅凭 `project_scans=0` 不能断言具体原因。诊断事件有数量上限；需要新一轮诊断时重启 AE。

工程内已存成 `??` 的文字仍无法通过编码转换恢复，需要未损坏的工程或原汉化插件重新生成缓存。
