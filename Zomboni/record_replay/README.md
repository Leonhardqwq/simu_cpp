# Zomboni CSV replay

把 damage-recorder CSV 转成冰车复现配置，并批量测试碾率。

```powershell
python Zomboni\record_replay\zomboni_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" build
# 编辑 scenario_profile.json、shroom_profile.csv
python Zomboni\record_replay\zomboni_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" run
python Zomboni\record_replay\zomboni_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" calc
```

目录输入同样支持：

```powershell
python Zomboni\record_replay\zomboni_replay.py "D:\AAA_LH_SL\DE46764" build
python Zomboni\record_replay\zomboni_replay.py "D:\AAA_LH_SL\DE46764" run
```

输出：

| 文件 | 用途 |
|---|---|
| `scenario_profile.json` | 全局 `num_test/test_type_plant` 与逐路 `crush_col/crush_type`。 |
| `shroom_profile.csv` | 可编辑喷曾列表。 |
| `zombonis_from_damage.csv` | 波次-路维度的预览和碾率结果；只编辑 `可忽略`。 |
| `zomboni_batch.json` | C++ 批量测试输入。 |
| `zomboni_rate_summary.csv` | `僵尸路,参与计算波次,平均碾率`。 |

规则：

- 只处理 `僵尸类型=冰车`。
- `伤害>=1800` 或 `植物类型=地刺/地刺王` 的冰车整只排除。
- case 按 `(僵尸波次, 僵尸路)` 分组。
- `冰瓜信息[冰瓜行,冰瓜列,最小溅射空间,最大溅射空间]` 为每个冰瓜的空间范围列表。
- 溅射空间按 C++ `melon_list` 语义反推：普通/南瓜/炮为 `僵尸坐标 - 80*crush_col - 40`，冰道为 `僵尸坐标 - 80*crush_col + 40`。
- 冰瓜空间当前取最小值写入 `melon_list`。

自检：

```powershell
python Zomboni\record_replay\test_zomboni_replay.py
```
