# MCPCBox Community Fix v7.0 · 记录持久化修正版（内部 v19）

对旧版盒子的官方版本下载、下载中心与共享缓存进行适配。本目录包含适配代码、两张内嵌页面、构建脚本及测试；主程序仍依赖用户原有盒子组件。

本次新增 [完成记录持久化修正](CHANGELOG-history.zh-CN.md)。此前下载时序修正见 [CHANGELOG-v18.zh-CN.md](CHANGELOG-v18.zh-CN.md)，页面与首次中文设置见 [CHANGELOG.zh-CN.md](CHANGELOG.zh-CN.md)。底层缓存和适配细节见 [README.source.md](README.source.md)。

## 构建

需要 i686 MinGW GCC、Python 3 和 pefile。在源码目录执行：

```sh
python3 -m pip install pefile
CC=i686-w64-mingw32-gcc bash build.sh --base /path/to/exact-v4/MCPCBox.exe --out-dir /path/to/output
```

输入必须是此前 v4 单列表 EXE，SHA-256：
`3f8209b850663b7e64e2244a5ecaf8f6e061bfea0975f00a02de11eaad08ccdc`。
脚本会验证输入、生成内嵌页面数组、编译载荷、合并导入与重定位并输出 EXE 和 validation.json。不要拿其他 EXE 套用私有接口偏移。原程序及 DLL 不包含在源码包中。

## 测试

解压后保持 native-v19 目录名，从其父目录执行：

```sh
bash native-v19/tests/verify-portable.sh
```

需要 Node.js、主机 GCC、OpenSSL 开发库及 pthread；OpenSSL 只用于主机缓存测试，不是 Windows 运行依赖。测试覆盖页面交互、官方安装计划、语言初始化、共享缓存和历史操作辅助逻辑。

机器码模拟测试另需 Python pefile、capstone、unicorn、构建后的 MCPCBox_NativeAdapter_v19/MCPCBox.exe，以及原始程序和匹配 DLL 测试素材。素材路径见 tests/test_adapter.py；这些二进制未放入源码包。具备素材后执行 tests/test_adapter.py 和 tests/test_entry.py。

## Windows 发布前验证

1. 退出旧盒子，仅替换 EXE，保留现有 DLL；检查页面在普通窗口、小窗口和实际 DPI 下的布局。
2. 下载一个版本，核对按钮的准备/下载状态，以及下载中心的一条整体任务。
3. 分别完成、取消、制造网络失败并重试；关闭结束提示，确认轮询不会重新显示，下一次相同版本任务仍可显示。
4. 在已下载页打开目录、多选仅清记录、删除 resources 中的普通资源文件，检查独立提示框、文件保留和批量结果。
5. 新装 1.8.9 和一个新正式版，首次启动核对简体中文；已有其他语言 options.txt 的版本重装后应保留原设置。
6. 再次下载核对缓存复用；退出重开后检查版本登记和文件完整性。

真实 Windows、内嵌 IE、游戏启动及安全软件复核尚待完成，不能把模拟测试当作实机验证。已知 360 告警未解决。

## 发布内容

测试包中包含 EXE、独立 Source-v18.zip、变更说明和验证结果。GitHub 源码提交可使用 Source-v18.zip 内的文件；测试包可作为测试版附件。包内未加入其他启动器源代码或新的外置运行插件。
