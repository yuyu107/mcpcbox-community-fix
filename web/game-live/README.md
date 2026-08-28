# 游戏直播兼容页

本目录记录 MCPCBox Community Fix v6.3 的“游戏直播”兼容页实现与部署关系。

## 实际部署

当前盒子内嵌浏览器加载：

`https://yuyu107.github.io/web/live/`

部署源码维护在：

`https://github.com/yuyu107/web/tree/master/live`

其中：

- `index.html`：直播推荐列表和直播间入口。
- `room.html`：旧 IE 内核兼容直播播放器。

## 主要兼容逻辑

- 通过虎牙公开房间接口取得当前直播流信息。
- 在页面本地按当前兼容算法重新计算 `wsSecret` 等参数。
- 优先尝试 HLS，已验证 HS CDN 在盒子内嵌浏览器中可以持续播放。
- FLV 作为备用线路。
- 对 `bufferStalledError` 等可恢复缓冲事件优先恢复，不立即切线。
- 网络错误先重试当前线路，持续失败后再切换 CDN。
- 提供隐藏式诊断信息，方便后续接口变化时排查。

## 为什么部署源码在 yuyu107/web

`MCPCBox.exe` 中旧直播 URL 是固定长度字符串。为了不破坏内嵌 UI XML，v6.3 使用与原地址完全等长的：

`https://yuyu107.github.io/web/live/`

因此实际在线页面继续部署在 `yuyu107/web`。本仓库保存补丁源码、版本说明和技术记录；在线网页源码仍可直接查看和修改。

## 第三方说明

兼容页会访问虎牙的公开网页接口和直播 CDN。接口、签名规则和 CDN 行为可能随虎牙更新而变化，本项目不保证第三方服务永久可用。
