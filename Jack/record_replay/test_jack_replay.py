from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace

import pandas as pd

from jack_replay import (
    ROW_DEFAULTS,
    SHROOM_TYPES,
    SHROOM_COLUMNS,
    SUMMARY_COLUMNS,
    app_dir,
    ash_info,
    build_cases,
    build_from_profiles,
    default_row_settings,
    default_base_config,
    default_runner_path,
    merge_splash_infos,
    parse_args,
    replay_time,
    resource_dir,
    read_damage_csv,
    safe_stem,
    summary_row,
    summarize_shrooms,
    wave_lengths,
)


def main() -> None:
    assert app_dir() == resource_dir() == Path(__file__).resolve().parent
    assert default_base_config() == Path(__file__).resolve().parents[2] / "Jack" / "jack_config.json"
    assert default_runner_path() == Path(__file__).resolve().parent / "Jack_record_replay.exe"
    lengths = {6: 1000, 7: 2000}
    assert replay_time(SimpleNamespace(wave=6, time=50), 6, lengths) == 50
    assert replay_time(SimpleNamespace(wave=8, time=50), 6, lengths) == 3050
    assert replay_time(SimpleNamespace(wave=5, time=50), 6, lengths) is None
    assert merge_splash_infos([(20, 26), (10, 80), (20, 40)]) == [[10, 80], [20, 66]]
    assert parse_args([r"D:\damage.csv"]).command == "build"
    assert parse_args([r"D:\damage.csv", "run"]).command == "run"
    assert ash_info(100, "玉米加农炮", "") == (100, 0, -1000, 1000)
    assert ash_info(100, "", "玉米炮") == (100, 0, -1000, 1000)
    assert ash_info(100, "缠绕海藻", "") == (100, 1, -1000, 1000)
    assert wave_lengths(pd.DataFrame({
        "wave": [1, 1, 2, 3],
        "time": [10, 600, 50, 10],
        "absolute_time": [1010, 1600, 1650, 2310],
    })) == {1: 601, 2: 700}
    assert SHROOM_COLUMNS == ["启用", "永动", "植物路", "植物列", "植物类型", "备注"]
    assert all(column not in SUMMARY_COLUMNS for column in ROW_DEFAULTS)
    assert all(column not in SUMMARY_COLUMNS for column in ["CI_lo", "CI_hi", "测试次数", "测试用时"])
    assert SUMMARY_COLUMNS[-7:] == ["最后伤害时机", "冰时机", "灰烬信息", "额外伤害", "冰瓜伤害", "溅射信息", "警告数量"]
    shrooms = pd.DataFrame(
        {"启用": [1, 1, 1], "植物路": [2, 3, 4], "植物列": [7, 6, 7]}
    )
    assert default_row_settings(3, shrooms)["boom_type"] == 1
    row = summary_row((6, 3, 123))
    assert row["僵尸波次"] == 6

    columns = [
        "波次", "时间", "绝对时间", "伤害", "僵尸ID", "僵尸类型", "僵尸路", "僵尸坐标", "僵尸波次",
        "僵尸总血量", "僵尸临界后", "子弹ID", "子弹类型", "植物ID", "植物类型", "植物路", "植物列",
    ]
    data = [
        [1, 2500, 2500, 1800, 123, "小丑", 3, 0, 1, 200, 1, 11, "玉米炮", 1, "", 3, 5],
        [1, 2520, 2520, 1800, 123, "小丑", 3, 0, 1, 200, 0, 0, "", 2, "樱桃炸弹", 3, 5],
        [1, 2600, 2600, 26, 123, "小丑", 3, 0, 1, 200, 0, 3, "冰瓜", 3, "冰西瓜投手", 3, 5],
        [1, 2650, 2650, 80, 123, "小丑", 3, 0, 1, 200, 1, 3, "冰瓜", 3, "冰西瓜投手", 3, 5],
        [1, 2700, 2700, 20, 123, "小丑", 3, 0, 1, 200, 0, 0, "", 4, "寒冰菇", 3, 5],
        [1, 2800, 2800, 40, 123, "小丑", 3, 0, 1, 200, 0, 0, "", 5, "大嘴花", 3, 5],
        [1, 2850, 2850, 30, 123, "小丑", 3, 0, 1, 200, 0, 0, "", 7, next(iter(SHROOM_TYPES)), 3, 5],
        [1, 2860, 2860, 30, 999, "路障", 5, 0, 1, 200, 0, 0, "", 8, next(iter(SHROOM_TYPES)), 5, 6],
        [2, 100, 3100, 80, 123, "小丑", 3, 0, 1, 200, 0, 6, "豌豆", 6, "豌豆射手", 3, 5],
        [2, 200, 3200, 80, 123, "小丑", 3, 0, 1, 200, 1, 6, "豌豆", 6, "豌豆射手", 3, 5],
    ]
    with TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        csv_path = tmp / "damage.csv"
        pd.DataFrame(data, columns=columns).to_csv(csv_path, index=False, encoding="utf-8-sig")
        df = read_damage_csv(csv_path)
        assert "source" not in df.columns
        assert "absolute_time" in df.columns
        assert "projectile_type" in df.columns
        assert "zombie_below_critical" in df.columns
        shroom_profile = summarize_shrooms(df, tmp)
        shroom_positions = set(
            map(tuple, shroom_profile[[SHROOM_COLUMNS[2], SHROOM_COLUMNS[3]]].astype(int).to_numpy())
        )
        assert (3, 5) in shroom_positions
        assert (5, 6) in shroom_positions

        base_config = tmp / "base_config.json"
        build_out = tmp / "build_out"
        base_config.write_text('{"scene": 1}', encoding="utf-8")
        with redirect_stdout(StringIO()):
            build_from_profiles(SimpleNamespace(
                csv_path=str(csv_path),
                out=str(build_out),
                base_config=str(base_config),
                num_test=None,
                limit=None,
            ))
        built = pd.read_csv(build_out / "jacks_from_damage.csv", encoding="utf-8-sig")
        assert (build_out / "jack_batch.json").exists()
        assert int(built.loc[0, "最后伤害时机"]) == 3100
        assert int(built.loc[0, "额外伤害"]) == 120

        batch_input = tmp / "batch_input"
        batch_input.mkdir()
        batch_csv = batch_input / "damage.csv"
        csv_path_2 = batch_input / "damage two.csv"
        pd.DataFrame(data, columns=columns).to_csv(batch_csv, index=False, encoding="utf-8-sig")
        pd.DataFrame(data, columns=columns).to_csv(csv_path_2, index=False, encoding="utf-8-sig")
        batch_out = tmp / "batch_out"
        with redirect_stdout(StringIO()):
            build_from_profiles(SimpleNamespace(
                csv_path=str(batch_input),
                out=str(batch_out),
                base_config=str(base_config),
                num_test=None,
                limit=None,
                sum=True,
            ))
        assert (batch_out / "scenario_profile.json").exists()
        assert (batch_out / "shroom_profile.csv").exists()
        assert (batch_out / safe_stem(batch_csv) / "jack_batch.json").exists()
        assert (batch_out / safe_stem(csv_path_2) / "jacks_from_damage.csv").exists()
        group_summary = pd.read_csv(batch_out / "jacks_from_damage.csv", encoding="utf-8-sig")
        assert group_summary.columns[-1] == "来源文件"
        assert set(group_summary["来源文件"]) == {"damage.csv", "damage two.csv"}

        cases = build_cases(
            df,
            csv_path,
            tmp,
            {
                "num_test": 100,
                "scene": 1,
                "vulnerable_time": 0,
                "test_type_zombie": 0,
                "test_type_plant": 0,
                "row_settings": {
                    "3": {
                        "boom_type": 2,
                        "boom_col": 5,
                        "defense_type": 0,
                        "jack_type": 0,
                        "ash_infos": [[3000, 1, -1000, 1000]],
                    }
                },
            },
            pd.DataFrame(columns=SHROOM_COLUMNS),
            wave_lengths(df),
            None,
        )
        config = cases[0]["config"]
        assert config["ash_infos"] == [
            [2500, 0, -1000, 1000],
            [2520, 1, -1000, 1000],
            [3000, 1, -1000, 1000],
        ]
        assert cases[0]["M_min"] == 3600
        assert config["splash_infos"] == [[2600, 26], [2650, 80]]
        assert config["extra_dmg"] == 120
        summary = pd.read_csv(tmp / "jacks_from_damage.csv", encoding="utf-8-sig")
        assert int(summary.loc[0, "最后伤害时机"]) == 3100
        assert summary.loc[0, "灰烬信息"] == "[[2500, 0, -1000, 1000], [2520, 1, -1000, 1000], [3000, 1, -1000, 1000]]"
        assert int(summary.loc[0, "额外伤害"]) == 120
        assert int(summary.loc[0, "冰瓜伤害"]) == 26
    print("record_replay self-check passed")


if __name__ == "__main__":
    main()
