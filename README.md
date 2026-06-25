# simu_cpp

## Release 打包

一键打包：

```powershell
.\build_release.ps1
```

如果 PowerShell 禁止直接运行脚本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_release.ps1
```

脚本会自动完成：

```text
创建/复用 .venv-release
安装 release 依赖
编译三个 C++ 主入口
打包三个单入口 exe
```

最终 exe 直接生成在各自功能目录：

```text
Jack/JackSimulator.exe
Enter/EnterSimulator.exe
Zomboni/ZomboniSimulator.exe
```

PyInstaller 中间产物统一放在：

```text
.pyinstaller/
```

发布给用户时，每个功能目录只需要保留入口 exe 和对应 Excel：

```text
JackSimulator.exe + jack_test_info.xlsx
EnterSimulator.exe + enter_test_info.xlsx
ZomboniSimulator.exe + zomboni_test_info.xlsx
```

运行时会在同目录生成配置和结果：

```text
jack_config.json / enter_config.json / zomboni_config.json
result.txt
```
