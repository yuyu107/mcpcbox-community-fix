# 更新记录

## v6.5

- 恢复“游戏管理 → 游戏纯净版下载”原生页面的数据链路。
- 将旧的 `gameAllRes.xml` 两个失效接口替换为 GitHub Pages 上的兼容 `gv.xml`。
- 首个恢复并完成从零安装、启动实测的纯净版为 Minecraft 1.7.10。
- 使用稳定版本 ID `1.7.10-mcbox`，列表显示名称为 `Minecraft 1.7.10`。
- 纯净版引导 ZIP 仅约 13 KB，不重新分发 Minecraft 官方客户端。
- 确认旧版启动器的 libraries 下载逻辑会按 `library.name` 生成 Maven 路径，并支持旧格式顶层 `library.url`。
- 利用旧格式 `library.url`，让盒子自身直接从 Mojang 官方地址获取 1.7.10 `client.jar`。
- 普通 libraries 继续从 `libraries.minecraft.net` 下载。
- 启动前置组件读取 GitHub 清单，并从 Mojang 官方资源服务补齐 1.7.10 语言资源。
- 首次启动默认设置为简体中文，同时保留后续手动切换语言的能力。
- 正式流程不再需要本地 HTTP 测试服务器。
- 更新 `tools/patch_mcpcbox.py` 与 `patches/v6.5.json`。
- v6.5 主程序 SHA-256 固定为 `578b7a054e977f2b8e15ebc97aae386ac440906b131065412e10afeb6dd3eb7a`。
- 通过全新 `Minecraft 1.7.10` 条目完成下载、安装和启动实测。

## v6.3

- 在 v6.2 基础上合并本轮已经实测通过的修复。
- 恢复“游戏直播”页面，入口改为 GitHub Pages 兼容页。
- 直播详情页使用当前虎牙签名方式生成播放地址。
- 优先使用 HLS/HS 线路，并加入多 CDN 切换、网络恢复、缓冲恢复和诊断信息。
- 已实测可在旧版盒子内嵌浏览器中连续播放当前虎牙《我的世界》直播。
- “查看全部”改为打开虎牙《我的世界》直播分类页。
- 顶部“多玩贴吧”按钮改为百度“多玩我的世界盒子吧”。
- “关于盒子”保留原版版本号，版权文字改为“原版版权”，宣传语改为“2026 社区兼容修复版”。
- 更新 `tools/patch_mcpcbox.py` 与 `patches/v6.3.json`，输出 SHA-256 固定为 `115ab85504084afa7b080400750b48205402835153f35cd93063284698a2cd7a`。
- 从本版起，源码仓库继续保存补丁和网页源码；Release 面向普通用户直接提供可替换的修复版主程序。

## v6.2

- 修复主界面“精彩视频”页面空白的问题。
- 将主界面实际使用的旧 `pcv3/index.html` 地址（5 处）替换为 GitHub Pages 兼容首页。
- 同时替换旧版布局使用的 `wonderfulvideo/videoIndex.html` 地址（3 处）。
- 页面不依赖原多玩网页服务，便于后续独立维护。

## v6.1

- 将 Minecraft 版本 JSON 基址替换为 GitHub Pages。
- 保留 v6.0 的 Java 7/8 自动下载修复。
- 下载仓库提供 1.6.2 至 1.16.5 的正式版 Windows 兼容 JSON。

## v6.0

- `jre7` 改用真正的 Azul Zulu OpenJDK 7u80 x64。
- `jre8` 使用 Azul Zulu OpenJDK 8u252 x64。
- 修正 Java 压缩包目录结构与 MD5。

## v5.8–v5.9

- Java 下载源改为 GitHub Raw。
- 修正压缩包多出一层 `java` 目录的问题。

## v5.6

- 增加中文入口和本地皮肤管理器。
- 支持列出、批量删除和备份本地皮肤索引。
