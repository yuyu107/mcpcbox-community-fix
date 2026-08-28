# MCPCBox Community Fix

旧版多玩我的世界盒子 PC 版的非官方兼容性修复与功能恢复项目。

本仓库保存可查看、可修改和可重复构建的脚本、补丁工具、网页源码与技术文档。Java 运行环境和 Minecraft 版本 JSON 由独立的 [`mcpcbox-downloads`](https://github.com/yuyu107/mcpcbox-downloads) 仓库提供。

## 当前版本

主分支已更新至 **v6.3**。

从 v6.3 起，仓库与 Release 分工如下：

- **仓库**：继续提供补丁脚本、补丁清单、网页源码和技术说明，方便查看、修改与重新构建。
- **Release**：提供已经修好的 `MCPCBox.exe` 和替换用 ZIP，普通用户下载后直接覆盖原文件即可。

源码仓库原则上不提交原版 MCPCBox 二进制。原版程序及第三方组件的版权仍归其原权利人所有。

## v6.3 已完成的修复

- 保留本地皮肤导入、中文提示与本地皮肤管理。
- Java 7u80、Java 8u252 x64 自动下载与识别。
- 将失效的 Java 下载地址替换为 GitHub Raw。
- 将失效的 Minecraft 版本 JSON 地址替换为 GitHub Pages。
- 提供 Minecraft 1.6.2 至 1.16.5 共 52 个正式版的 Windows 兼容 JSON。
- 过滤会被旧启动器错误解析的非 Windows 依赖。
- 将失效的“精彩视频”网页替换为可维护的兼容页面。
- 恢复“游戏直播”页面，并通过兼容播放器播放当前虎牙《我的世界》直播。
- 直播播放器优先使用已验证稳定的 HLS/HS 线路，支持多线路切换、缓冲恢复和诊断信息。
- “查看全部”改为在浏览器中打开虎牙《我的世界》直播分类页。
- 顶部“多玩贴吧”改为百度“多玩我的世界盒子吧”。
- “关于盒子”保留原版本号，并增加“2026 社区兼容修复版”标识。

## 仓库内容

- `src/local-skin-manager/`：本地皮肤导入和管理脚本源码。
- `tools/patch_mcpcbox.py`：将受支持的 v5.6 `MCPCBox.exe` 更新为 v6.3。
- `patches/v6.3.json`：v6.3 的字符串补丁清单和校验值。
- `web/wonderful-video/`：“精彩视频”兼容页面源码。
- `web/game-live/`：“游戏直播”兼容页源码与部署说明。
- `docs/BUILDING.md`：构建和验证方法。
- `CHANGELOG.md`：版本更新记录。

## 游戏直播网页

当前实际部署地址为：

- `https://yuyu107.github.io/web/live/`

实际部署源码维护在 `yuyu107/web` 仓库的 `live/` 目录；本仓库的 `web/game-live/` 用于保存该功能的源码说明和版本对应关系。

## 使用说明

普通用户优先从 Releases 下载已经修好的成品：

1. 完全退出多玩我的世界盒子。
2. 备份原来的 `MCPCBox.exe`。
3. 用 Release 中的修复版 `MCPCBox.exe` 覆盖原文件。
4. 重新启动盒子。

需要自行构建时，请按 `docs/BUILDING.md` 使用 `tools/patch_mcpcbox.py`，并确认输入文件 SHA-256 与补丁脚本要求一致。

## 许可证与第三方内容

MIT License 仅适用于本项目自行编写的脚本、补丁工具和文档，不适用于原版 MCPCBox、Minecraft、Azul Zulu、虎牙网页/接口或其他第三方组件。项目不隶属于多玩、YY、Mojang、Microsoft、Azul 或虎牙。
