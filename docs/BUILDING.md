# 构建 v6.5

## 环境

- Python 3.8 或更高版本。
- 已合法取得的 v5.6 测试版 `MCPCBox.exe`。

## 生成主程序

在仓库根目录运行：

```powershell
python tools\patch_mcpcbox.py input\MCPCBox.exe build\MCPCBox.exe
```

工具会先验证输入 SHA-256，只接受补丁清单指定的 v5.6 文件；修改完成后还会验证输出是否与 v6.5 一致。

v6.5 预期输出 SHA-256：

`578b7a054e977f2b8e15ebc97aae386ac440906b131065412e10afeb6dd3eb7a`

## v6.5 新增的纯净版入口补丁

除此前 Java、Minecraft JSON、精彩视频、游戏直播、贴吧和关于页修复外，v6.5 还替换了两个旧纯净版列表接口：

- `http://pc.mcapi.tuboshu.com/api/pc/res/xml/gameAllRes.xml`
- `http://pc.mcapi.tuboshu.com/api/pc/gameRes/v2/xml/gameAllRes.xml`

两者都指向：

`https://yuyu107.github.io/mcpcbox-downloads/gv.xml`

纯净版的版本 JSON、引导 ZIP 和在线清单由 `mcpcbox-downloads` 仓库维护，不需要写入主程序二进制。

## 组合测试目录

将生成的 `build\MCPCBox.exe` 与用户已合法取得的原版盒子运行文件放在同一安装目录即可。

本地皮肤管理器是独立工具，不是 v6.5 主程序运行所必需的组件。

源码仓库不提供原版盒子、Minecraft 客户端或其他第三方二进制文件。
