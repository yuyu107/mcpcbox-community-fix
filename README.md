# MCPCBox Community Fix

旧版「多玩我的世界盒子 PC 版」的非官方社区兼容修复与功能恢复项目。

本仓库主要保存 **补丁源码、补丁清单、兼容网页源码与技术文档**；普通用户可以直接从 Releases 下载已经修好的成品。

> 本项目不是多玩官方版本，也不隶属于多玩、YY、Mojang、Microsoft、虎牙或其他相关厂商。

## 下载

### MCPCBox Community Fix v6.5

主程序兼容修复版：

- [Release：MCPCBox Community Fix v6.5](https://github.com/yuyu107/mcpcbox-community-fix/releases/tag/v6.5)
- 推荐下载：`MCPCBox_Community_Fix_v6.5.zip`
- 也可单独下载：`MCPCBox_Community_Fix_v6.5.exe`

使用时完全退出盒子，备份原 `MCPCBox.exe`，然后用 Release 中的修复版覆盖即可。

原版盒子本身一直支持**导入本地已有的 Minecraft 游戏目录并启动游戏**；v6.5 新恢复的是此前因在线接口失效而无法使用的**“游戏纯净版下载”在线下载安装功能**。如果本机已经有可用游戏，可以继续按原版方式导入和启动，不需要重新下载。

### MCPCBox Local Skin Manager v1.0

本地皮肤导入/管理工具从主程序中独立发布：

- [Release：MCPCBox Local Skin Manager v1.0](https://github.com/yuyu107/mcpcbox-community-fix/releases/tag/skin-manager-v1.0)
- 下载：`MCPCBox_Local_Skin_Manager_v1.0.zip`

皮肤工具不需要替换主程序，可以单独使用。

## 当前主程序版本：v6.5

v6.5 基于原版 `2.0.1429734.498` 进行兼容修复，目前包括：

- **恢复“游戏管理 → 游戏纯净版下载”的在线下载安装功能**。
- 原版已有的本地游戏导入、管理与启动能力保持不变。
- 纯净版列表改由 GitHub Pages 提供，不再依赖已失效的多玩资源接口。
- 首个恢复并实测通过的纯净版为 **Minecraft 1.7.10**。
- 纯净版引导包不包含 Minecraft 官方客户端 JAR。
- 启动时由旧版 MCPCBox 自身从 Mojang 官方地址下载 Minecraft 1.7.10 客户端。
- 普通 Minecraft libraries 继续从 `libraries.minecraft.net` 获取。
- 首次启动自动补齐 1.7.10 语言资源并默认简体中文；之后用户仍可自行修改语言。
- 整个纯净版安装与启动流程不需要本地 HTTP 服务器。
- 修复失效的 Java 运行环境下载地址。
- 支持 Java 7u80 / Java 8u252 x64 的兼容下载与识别。
- 修复 Minecraft 版本 JSON 下载地址。
- 配套提供 Minecraft 1.6.2 至 1.16.5 共 52 个正式版的 Windows 兼容 JSON。
- 过滤旧启动器会错误处理的非 Windows 依赖。
- 恢复“精彩视频”页面。
- 恢复“游戏直播”页面。
- 在盒子内通过兼容播放器播放当前虎牙《我的世界》直播。
- 直播播放器使用 HLS 优先策略，支持多 CDN 切换、缓冲恢复和诊断信息。
- “查看全部直播”跳转到虎牙《我的世界》直播分类页。
- 顶部“多玩贴吧”按钮改为百度“多玩我的世界盒子吧”。
- “关于盒子”保留原版版本号，并标记为“2026 社区兼容修复版”。

## 游戏启动与纯净版下载

原版 MCPCBox 的**本地游戏导入与启动功能本身仍然可用**。如果电脑中已经有可用的 Minecraft 游戏目录，可以继续通过盒子的游戏管理功能导入并启动，不需要使用纯净版下载。

v6.5 额外恢复了原生 `pure_version_itembox` 页面使用的**在线纯净版下载链路**，用于直接从盒子里下载安装游戏版本。

当前稳定在线下载条目：

- 显示名称：`Minecraft 1.7.10`
- 版本 ID：`1.7.10-mcbox`
- 纯净版清单：`mcpcbox-downloads/gv.xml`
- 引导 ZIP：`mcpcbox-downloads/pure/minecraft_1.7.10_bootstrap.zip`

恢复方案保留旧盒子的原生安装、依赖检查和启动流程。Minecraft 客户端及 Mojang 资源文件不由本项目重新分发，而是由用户本机从官方服务获取。

## 本地皮肤工具

`MCPCBox Local Skin Manager` 为独立附加工具，源码位于：

`src/local-skin-manager/`

当前功能包括：

- 导入 64×64 和 64×32 PNG 皮肤。
- 支持经典 Steve 与纤细 Alex 手臂模型。
- 64×64 皮肤自动生成 `[1.8+]` 与 `[1.7]` 两个兼容条目。
- 针对盒子旧版渲染器处理透明度、叠加层和手臂 UV 兼容问题。
- 中文界面的本地皮肤管理与批量删除。
- 删除前自动备份 `downloaded_res.xml`。

普通用户请直接下载独立的 `skin-manager-v1.0` Release。

## 仓库内容

- `tools/patch_mcpcbox.py`：从受支持的原版主程序生成当前修复版。
- `patches/v6.5.json`：v6.5 字符串补丁清单、替换目标与 SHA-256。
- `src/local-skin-manager/`：本地皮肤工具源码。
- `web/wonderful-video/`：“精彩视频”兼容页面源码。
- `web/game-live/`：“游戏直播”兼容页说明与源码对应关系。
- `docs/BUILDING.md`：自行构建与验证方法。
- `docs/RELEASE_v6.5.md`：v6.5 发布说明模板。
- `docs/RELEASE_local_skin_manager_v1.0.md`：皮肤工具发布说明模板。
- `CHANGELOG.md`：项目更新记录。

## 配套资源仓库

Java 运行环境、Minecraft 版本 JSON、纯净版清单和引导资源由独立仓库提供：

[`yuyu107/mcpcbox-downloads`](https://github.com/yuyu107/mcpcbox-downloads)

该仓库主要作为旧版盒子的替代下载源和 GitHub Pages 静态资源源使用。

游戏直播兼容页当前部署在：

`https://yuyu107.github.io/web/live/`

实际在线页面由 `yuyu107/web` 仓库维护。

## 自行构建

如果不使用 Release 成品，可以使用：

`tools/patch_mcpcbox.py`

对受支持的原版 `MCPCBox.exe` 应用补丁。脚本会检查输入文件 SHA-256，并在生成后再次校验输出文件，避免对错误版本误打补丁。

详细说明见：

`docs/BUILDING.md`

## 项目结构说明

从 v6.3 开始：

- **Git 仓库**：保存可查看、可修改、可重建的源码和补丁资料。
- **Releases**：保存普通用户可直接使用的成品包。
- **mcpcbox-downloads**：保存旧下载接口所需的替代资源。
- **web**：承载需要持续在线维护的兼容网页。

这样可以把程序补丁、运行资源和在线网页分别维护，后续某一个服务失效时不需要重新整理整个项目。

## 许可证与第三方内容

本仓库的 MIT License 仅适用于本项目自行编写的脚本、补丁工具、网页兼容代码和文档。

它不适用于原版 MCPCBox、Minecraft、Java 运行环境、虎牙内容或其他第三方组件。相关软件、商标及内容版权归各自权利人所有。
