# MCPCBox Community Fix v6.5

基于旧版多玩我的世界盒子 PC 版的社区兼容修复。

## 本版主要更新

- 恢复“游戏管理 → 游戏纯净版下载”的在线下载安装功能。
- 原版已有的**本地游戏导入、管理与启动能力保持不变**；本机已有可用 Minecraft 时，可以继续按原版方式导入并启动，不需要重新下载。
- 纯净版列表从 GitHub Pages 获取，不再依赖失效的多玩接口。
- 首个恢复版本为 **Minecraft 1.7.10**，已完成全新安装和启动实测。
- 引导安装包仅约 13 KB，不包含 Minecraft 官方客户端。
- Minecraft 1.7.10 `client.jar` 由旧版 MCPCBox 自己从 Mojang 官方地址下载。
- 普通 libraries 继续从 `libraries.minecraft.net` 下载。
- 首次启动自动补齐语言资源并默认简体中文。
- 正式在线安装/启动流程无需本地 HTTP 服务器。
- 保留此前 Java 下载、Minecraft JSON、精彩视频、游戏直播、贴吧和关于页等修复。

## Release 建议附件

- `MCPCBox_Community_Fix_v6.5.exe`
- `MCPCBox_Community_Fix_v6.5.zip`

## 使用方法

1. 完全退出多玩我的世界盒子。
2. 建议备份安装目录中的原 `MCPCBox.exe`。
3. 下载 Release 中的修复版 `MCPCBox.exe`，覆盖原文件。
4. 重新启动盒子。
5. 如果本机已经有可用 Minecraft，可继续通过盒子的游戏管理功能导入本地游戏并直接启动。
6. 如果希望由盒子在线安装游戏，则进入“游戏管理 → 游戏纯净版下载”。
7. 当前可在线下载安装 `Minecraft 1.7.10`，安装完成后可直接启动。

## 文件校验

`MCPCBox_Community_Fix_v6.5.exe`

SHA-256：

`578b7a054e977f2b8e15ebc97aae386ac440906b131065412e10afeb6dd3eb7a`

`MCPCBox_Community_Fix_v6.5.zip`

SHA-256：

`93db1558c3bee4fcee508a962c26b256a916fd7fd269d62adcf9cd5807933527`

## 纯净版资源说明

原版 MCPCBox 的本地导入与启动功能本身并未被本项目替代；v6.5 恢复的是此前因在线接口失效而无法使用的“游戏纯净版下载”链路。

当前稳定在线下载条目：

- 显示名称：`Minecraft 1.7.10`
- 版本 ID：`1.7.10-mcbox`
- 清单：`https://yuyu107.github.io/mcpcbox-downloads/gv.xml`
- 引导 ZIP：`https://yuyu107.github.io/mcpcbox-downloads/pure/minecraft_1.7.10_bootstrap.zip`

本项目不重新分发 Minecraft 官方客户端 JAR 或 Mojang 官方资源文件；它们由用户本机从官方服务获取。

## 说明

本项目为非官方社区兼容修复，不隶属于多玩、YY、Mojang、Microsoft 或虎牙。原版程序及第三方组件版权归各自权利人所有。
