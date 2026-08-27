# 构建 v6.2

## 环境

- Python 3.8 或更高版本。
- 已合法取得的 v5.6 测试版 `MCPCBox.exe`。

## 生成主程序

在仓库根目录运行：

```powershell
python tools\patch_mcpcbox.py input\MCPCBox.exe build\MCPCBox.exe
```

工具会先验证输入 SHA-256，只接受补丁清单指定的 v5.6 文件；修改完成后还会验证输出是否与 v6.2 一致。

## 组合测试目录

将生成的 `build\MCPCBox.exe` 与以下文件放在同一目录：

- v5.6 的 `mcoption.dll`；
- `src\local-skin-manager\local_skin_library_import.ps1`；
- `src\local-skin-manager\local_skin_library_import.cmd`；
- 用户已合法取得的原版盒子运行文件。

源码仓库不提供原版盒子二进制文件。
