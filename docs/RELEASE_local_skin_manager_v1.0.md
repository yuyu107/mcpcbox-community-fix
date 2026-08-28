# MCPCBox Local Skin Manager v1.0

这是 `MCPCBox Community Fix` 的独立附加工具发布包，不与主程序 `MCPCBox.exe` 替换包合并。

## Release 附件

建议附件名：

- `MCPCBox_Local_Skin_Manager_v1.0.zip`

发布 ZIP 内包含：

- `启动本地皮肤工具.cmd`
- `local_skin_library_import.ps1`
- `说明.txt`

## 功能

- 导入 64×64 与 64×32 PNG Minecraft 皮肤。
- 64×64 皮肤可选择纤细/Alex（3 像素手臂）或经典/Steve（4 像素手臂）。
- 64×64 皮肤自动生成 `[1.8+]` 64×64 与 `[1.7]` 64×32 两个兼容条目。
- 针对旧版 MCPCBox 预览器处理透明度、身体叠加层及手臂 UV 兼容问题。
- 提供中文“管理本地皮肤”界面，可批量删除本工具添加的皮肤。
- 删除前自动备份 `downloaded_res.xml` 为 `downloaded_res.xml.before-delete.bak`。
- 原始用户皮肤 PNG 不会被修改。

## 使用方法

1. 建议先完全退出多玩我的世界盒子。
2. 解压发布包。
3. 双击 `启动本地皮肤工具.cmd`。
4. 选择“是”导入皮肤，选择“否”管理或删除已导入皮肤。
5. 操作完成后重新打开盒子。

本地皮肤保存到：

`%APPDATA%\duowan\mcpcbox\resources\local_skins`

资源索引：

`%APPDATA%\duowan\mcpcbox\config\downloaded_res.xml`

## 源码

源码继续保存在：

`src/local-skin-manager/`

Release 仅提供方便普通用户直接使用的独立 ZIP 包。