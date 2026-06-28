# Jack CSV replay

将 damage-recorder CSV 转成小丑复现配置，并调用 `JackConfig` / `work` 批量测试炸率。

## 使用

CSV 必须使用绝对路径。正式流程：

```powershell
python Jack\record_replay\jack_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" build
# 编辑 scenario_profile.json、shroom_profile.csv
python Jack\record_replay\jack_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" run
# 修改“可忽略”后，无需重新测试即可重算分路炸率
python Jack\record_replay\jack_replay.py "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" calc
```

默认输出目录为 `Jack/record_replay/out/<csv-stem>/`。使用 `-h` 查看 `--out`、`--num-test`、`--limit`、`--build` 等参数。

输入也可以是 CSV 所在目录：

```powershell
python Jack\record_replay\jack_replay.py "D:\AAA_LH_SL\DE46764" build
python Jack\record_replay\jack_replay.py "D:\AAA_LH_SL\DE46764" run
```

目录模式会在根输出目录生成共用 `scenario_profile.json` / `shroom_profile.csv`，每个 CSV 的 `jacks_from_damage.csv` 和 `jack_batch.json` 放在独立子目录。

目录模式会在根输出目录生成合并后的 `jacks_from_damage.csv`；`来源文件` 位于最后一列。

CSV 使用新版 damage-recorder 输出：

```text
波次,时间,绝对时间,伤害,僵尸ID,僵尸类型,僵尸路,僵尸坐标,僵尸波次,僵尸总血量,僵尸临界后,子弹ID,子弹类型,植物ID,植物类型,植物路,植物列
```

| 命令 | 作用 |
|---|---|
| `build` | 解析 CSV，生成可编辑配置、完整预览表和 `jack_batch.json`。 |
| `run` | 先按当前配置重新 build，再测试；每完成一个 case，立即将结果写回摘要。 |
| `calc` | 只读取现有摘要并重算分路炸率，不解析伤害表、不运行模拟。 |

| 输出文件 | 用途 | 是否编辑 |
|---|---|---:|
| `scenario_profile.json` | 全局测试条件及分路设置。 | 是 |
| `shroom_profile.csv` | 喷曾配置。 | 是 |
| `jacks_from_damage.csv` | 小丑信息、时机和炸率结果。 | 仅编辑 `可忽略` |
| `jack_batch.json` | C++ 批量测试输入。 | 否 |
| `jack_rate_summary.csv` | 按路统计的参与数量和平均炸率。 | 否 |

## `scenario_profile.json`

顶层字段作用于所有小丑。已有文件会保留编辑值，`scene` 首次从 `Jack/jack_config.json` 读取。

| 字段 | 默认值 | 含义 |
|---|---:|---|
| `num_test` | `10000` | 每只小丑的模拟次数。 |
| `scene` | `1` | `0=前院`，`1=后院`，`2=屋顶`。 |
| `vulnerable_time` | `0` | 目标植物开始可伤的时机。 |
| `test_type_zombie` | `0` | 小丑速度：`0=随机`，`1=最快`，`2=最慢`。 |
| `test_type_plant` | `0` | 喷曾攻击时机：`0=随机`，`1=最快`，`2=最慢`。 |

批量入口固定使用 `show_progress=false`。`extra_dmg` 由 CSV 自动计算。

`row_settings` 按伤害表中实际存在小丑的路数生成，每一路设置作用于该路全部小丑：

```json
"row_settings": {
  "1": {
    "boom_type": 0,
    "boom_col": 5,
    "defense_type": 0,
    "jack_type": 0,
    "ash_infos": []
  }
}
```

| 字段 | 含义 |
|---|---|
| `boom_type` | `0=上炸下`，`1=下炸上`，`2=直接炸目标`。 |
| `boom_col` | 被炸目标所在列。 |
| `defense_type` | `0=普通植物`，`1=南瓜`，`2=玉米炮`。 |
| `jack_type` | `0=全部`，`1=早爆`，`2=晚爆`。 |
| `ash_infos` | 人工追加灰烬列表：`[时机, 是否卡片灰烬, 左边界, 右边界]`。 |

`ash_infos` 的第二项为 `1` 时表示卡片灰烬，为 `0` 时表示炮灰烬。CSV 中 `伤害 >= 1800` 的灰烬/即死记录会自动生成灰烬信息，范围固定为 `[-1000, 1000]`；`子弹类型=玉米炮` 或 `植物类型=玉米加农炮` 记为炮灰烬，其余记为卡片灰烬。人工 `ash_infos` 会追加到自动灰烬之后。

`hugewave` 自动按小丑波次生成：第 10、20 波为 `true`，其余为 `false`。

`boom_col`、`boom_type` 默认值由启用的喷曾生成：检索小丑同路及上下路植物，先按植物列从大到小，再按“同路、上路、下路”排序。首项植物列作为 `boom_col`；同路、上路、下路分别对应 `boom_type=2、1、0`。

## `shroom_profile.csv`

脚本从整张伤害表中出现过伤害的大喷菇/忧郁菇汇总植物，并按“植物类型 + 植物路 + 植物列”保留已有编辑。

| 列 | 含义 |
|---|---|
| `启用` | `1=启用`，`0=忽略`。 |
| `永动` | `1=永动`，`0=等待触发后工作`。 |
| `植物路` | 植物所在路。 |
| `植物列` | 植物所在列。 |
| `植物类型` | `大喷菇` 或 `忧郁菇`。 |
| `备注` | 备注，不参与模拟。 |

大喷菇只作用于同路小丑；忧郁菇作用于路差不超过 1 的小丑。脚本内部转换为 `[植物列, cpp_type, 永动]`，其中 `大喷菇=1`、`忧郁菇=0`。

## `jacks_from_damage.csv`

一行对应一只小丑，结果通过“僵尸 ID + 波次 + 路数”写回。

| 字段 | 含义 |
|---|---|
| `炸率` | 炸率，范围 `0~1`。 |
| `error` | case 失败时的错误信息。 |
| `可忽略` | 留空时参与统计；填 `1` 时不参与分路炸率统计。 |
| `僵尸ID`、`僵尸波次`、`僵尸路` | 小丑标识。 |
| `最后伤害时机` | `僵尸临界后=0` 的最晚伤害记录对应的相对时机。 |
| `冰时机` | 寒冰菇生成的冰时机，同一时机去重。 |
| `灰烬信息` | 自动灰烬与人工追加灰烬合并后的 `ash_infos`。 |
| `额外伤害` | 临界前伤害中，排除寒冰菇和冰瓜后剩余伤害的总和，写入 `extra_dmg`。 |
| `冰瓜伤害` | 临界前 `子弹类型=冰瓜` 的伤害总和。 |
| `溅射信息` | `子弹类型=冰瓜` 的伤害 `[时机, 伤害]`，同一时机合并，临界后也保留。 |
| `警告数量` | 因波次异常或缺少波长而跳过的伤害记录数。 |

跨波时机使用 CSV `绝对时间 - 时间` 推导内部波长，波长最小为 `601`；用户不需要编辑波长文件。

`build` 按当前配置写入时机和 batch，`run` 会先重算再填入结果。
`jack_rate_summary.csv` 按 `僵尸路` 对有效炸率取平均；空炸率和 `可忽略=1` 的记录不参与。目录模式下 `calc` 同时计算各子目录和根目录合并表。

## `jack_batch.json`

由 `build` / `run` 自动生成，通常无需编辑。每个 case 包含：

| 字段 | 含义 |
|---|---|
| `case_id` | case 标识。 |
| `meta` | CSV 路径及小丑 ID、波次、路数。 |
| `config` | `scenario_profile.json` 与 CSV 推导结果合成的 `JackConfig`。 |
| `M_min` | 覆盖最晚事件所需的最小模拟时长。 |

`config` 自动加入 `hugewave`、`ice_t`、`splash_infos`、`ash_infos`、`extra_dmg` 和 `plant_list`，并带入当前行的爆炸、开盒及人工追加灰烬设置。

## 打包

局部打包脚本和中间产物都放在 `Jack/record_replay/release/`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Jack\record_replay\release\build_record_replay.ps1
```

脚本会创建/复用 `release/.venv/`，编译内置 `Jack_record_replay.exe`，并生成：

```text
Jack/record_replay/release/JackRecordReplay.exe
```

打包后的用法：

```powershell
Jack\record_replay\release\JackRecordReplay.exe "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" build
Jack\record_replay\release\JackRecordReplay.exe "D:\AAA_LH_SL\DE46764\Records-2026-06-26-18-43-38.csv" run
```

## 自检

```powershell
python Jack\record_replay\test_jack_replay.py
```
