# JackRecordReplay 使用说明

```powershell
# 1. 生成配置和预览
.\JackRecordReplay.exe "D:\AAA_LH_SL\DE46764" build

# 2. 编辑 out\DE46764\scenario_profile.json 和 shroom_profile.csv 后运行测试
.\JackRecordReplay.exe "D:\AAA_LH_SL\DE46764" run

# 修改“可忽略”后，只重新计算汇总炸率
.\JackRecordReplay.exe "D:\AAA_LH_SL\DE46764" calc
```

输入可以是单个伤害 CSV 的绝对路径，也可以是包含多个 CSV 的目录。结果保存在 exe 同目录的 `out\<输入名称>\` 中。
