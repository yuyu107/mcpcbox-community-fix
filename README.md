# MCPCBox Community Fix

旧版多玩我的世界盒子 PC 版的非官方兼容性修复与功能恢复项目。

本仓库保存可查看、可修改和可重复构建的脚本、补丁工具与技术文档。Java 运行环境和 Minecraft 版本 JSON 由独立的 [`mcpcbox-downloads`](https://github.com/yuyu107/mcpcbox-downloads) 仓库提供。

## 已完成的修复

- 本地皮肤导入、中文提示与本地皮肤管理。
- Java 7u80、Java 8u252 x64 自动下载与识别。
- 将失效的 Java 下载地址替换为 GitHub Raw。
- 将失效的版本 JSON 地址替换为 GitHub Pages。
- 提供 Minecraft 1.6.2 至 1.16.5 共 52 个正式版的 Windows 兼容 JSON。
- 过滤会被旧启动器错误解析的非 Windows 依赖。
- 将失效的“精彩视频”网页替换为可维护的兼容页面。

## 仓库内容

- `src/local-skin-manager/`：本地皮肤导入和管理脚本源码。
- `tools/patch_mcpcbox.py`：将 v5.6 的 `MCPCBox.exe` 更新为 v6.2。
- `patches/v6.2.json`：v6.2 的字符串补丁清单和校验值。
- `docs/BUILDING.md`：构建和验证方法。
- `CHANGELOG.md`：版本更新记录。

## 使用说明

当前测试成品将在本仓库的 Releases 页面提供。源码仓库不包含原版 MCPCBox 程序；构建补丁时需要用户自行提供已合法取得的目标文件。

## 许可证与第三方内容

MIT License 仅适用于本项目自行编写的脚本、补丁工具和文档，不适用于原版 MCPCBox、Minecraft、Azul Zulu 或其他第三方组件。项目不隶属于多玩、YY、Mojang、Microsoft 或 Azul。
