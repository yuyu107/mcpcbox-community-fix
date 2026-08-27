# 下载服务修复

## Java

程序的 64 位 Java 地址被替换为 `mcpcbox-downloads` 仓库中的 Raw 文件。压缩包根目录直接包含 `jre7` 与 `jre8`：

- `jre7`：Azul Zulu OpenJDK 7u80 x64；
- `jre8`：Azul Zulu OpenJDK 8u252 x64。

## Minecraft 版本 JSON

旧 S3 基址被替换为 GitHub Pages。地址格式为：

```text
https://yuyu107.github.io/mcpcbox-downloads/<版本>/<版本>.json
```

下载仓库保存 1.6.2 至 1.16.5 的 52 个正式版 JSON。它们以 Mojang 官方 JSON 为基础，并过滤明确不适用于 Windows x64 的 library 条目。

## 已有版本的 JSON

盒子不会自动覆盖游戏目录中已有的版本 JSON。必要时应同时更新：

```text
%APPDATA%\duowan\mcpcbox\mcoption\json\<版本>.json
.minecraft\versions\<版本>\<版本>.json
```

