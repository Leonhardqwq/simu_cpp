from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace

import json

import pandas as pd

from zomboni_replay import (
    IGNORE_COLUMN,
    SUMMARY_COLUMNS,
    build_cases,
    build_from_profiles,
    calculate_rates,
    default_base_config,
    default_runner_path,
    melon_info_for_group,
    MELON_INFO_COLUMN,
    parse_args,
    read_damage_csv,
    resource_dir,
    app_dir,
    safe_stem,
    summarize_shrooms,
    valid_zomboni_damage,
    write_rate_summary,
)


def main() -> None:
    assert app_dir() == resource_dir() == Path(__file__).resolve().parent
    assert default_base_config() == Path(__file__).resolve().parents[2] / "Zomboni" / "zomboni_config.json"
    assert default_runner_path() == Path(__file__).resolve().parent / "Zomboni_record_replay.exe"
    assert parse_args([r"D:\damage.csv"]).command == "build"
    assert parse_args([r"D:\damage.csv", "run"]).command == "run"
    assert parse_args([r"D:\damage.csv", "calc"]).command == "calc"
    assert SUMMARY_COLUMNS == ["碾率", "error", "可忽略", "僵尸波次", "僵尸路", MELON_INFO_COLUMN]

    columns = [
        "波次", "时间", "绝对时间", "伤害", "僵尸ID", "僵尸类型", "僵尸路", "僵尸坐标", "僵尸波次",
        "僵尸总血量", "僵尸临界后", "子弹ID", "子弹类型", "植物ID", "植物类型", "植物路", "植物列",
    ]
    data = [
        [1, 100, 100, 26, 1, "冰车", 3, 520, 1, 1350, 0, 10, "冰瓜", 101, "冰西瓜投手", 3, 5],
        [1, 120, 120, 26, 1, "冰车", 3, 480, 1, 1350, 0, 10, "冰瓜", 101, "冰西瓜投手", 3, 5],
        [1, 110, 110, 26, 2, "冰车", 3, 500, 1, 1350, 0, 10, "冰瓜", 101, "冰西瓜投手", 3, 5],
        [1, 130, 130, 1800, 3, "冰车", 3, 500, 1, 0, 1, 0, "", 102, "樱桃炸弹", 3, 5],
        [1, 140, 140, 20, 4, "冰车", 3, 500, 1, 1350, 0, 0, "", 103, "地刺", 3, 5],
        [1, 150, 150, 20, 999, "路障", 3, 0, 1, 200, 0, 0, "", 104, "忧郁菇", 3, 7],
    ]

    with TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        csv_path = tmp / "damage.csv"
        pd.DataFrame(data, columns=columns).to_csv(csv_path, index=False, encoding="utf-8-sig")
        df = read_damage_csv(csv_path)
        assert len(valid_zomboni_damage(df)["zombie_id"].unique()) == 2
        assert melon_info_for_group(valid_zomboni_damage(df), 7, 0)[0] == [[3, 5, -100, -80]]
        assert melon_info_for_group(valid_zomboni_damage(df), 7, 2)[0] == [[3, 5, -20, 0]]

        base_config = tmp / "base_config.json"
        base_config.write_text('{"crush_col": 6}', encoding="utf-8")
        out = tmp / "out"
        with redirect_stdout(StringIO()):
            build_from_profiles(SimpleNamespace(
                csv_path=str(csv_path),
                out=str(out),
                base_config=str(base_config),
                num_test=None,
                limit=None,
            ))
        summary = pd.read_csv(out / "zombonis_from_damage.csv", encoding="utf-8-sig")
        batch = json.loads((out / "zomboni_batch.json").read_text(encoding="utf-8"))
        assert len(batch["cases"]) == 1
        assert summary.loc[0, MELON_INFO_COLUMN] == "[[3, 5, -100, -80]]"
        assert batch["cases"][0]["config"]["crush_col"] == 7
        assert batch["cases"][0]["config"]["melon_list"] == [[-100, 2, 1]]
        assert batch["cases"][0]["config"]["shroom_list"] == [[7, 0, 1]]

        scenario = json.loads((out / "scenario_profile.json").read_text(encoding="utf-8"))
        scenario["row_settings"]["3"]["crush_type"] = 2
        (out / "scenario_profile.json").write_text(json.dumps(scenario, ensure_ascii=False, indent=2), encoding="utf-8")
        with redirect_stdout(StringIO()):
            build_from_profiles(SimpleNamespace(
                csv_path=str(csv_path),
                out=str(out),
                base_config=str(base_config),
                num_test=None,
                limit=None,
            ))
        batch = json.loads((out / "zomboni_batch.json").read_text(encoding="utf-8"))
        assert batch["cases"][0]["config"]["melon_list"] == [[-20, 2, 1]]

        summary = pd.read_csv(out / "zombonis_from_damage.csv", encoding="utf-8-sig")
        summary["碾率"] = [0.2]
        summary[IGNORE_COLUMN] = [""]
        summary.to_csv(out / "zombonis_from_damage.csv", index=False, encoding="utf-8-sig")
        rates = pd.read_csv(write_rate_summary(out), encoding="utf-8-sig")
        assert rates.to_dict("records") == [{"僵尸路": 3, "参与计算波次": 1, "平均碾率": 0.2}]
        summary[IGNORE_COLUMN] = [1]
        summary.to_csv(out / "zombonis_from_damage.csv", index=False, encoding="utf-8-sig")
        with redirect_stdout(StringIO()):
            calculate_rates(SimpleNamespace(csv_path=str(csv_path), out=str(out)))
        assert pd.read_csv(out / "zomboni_rate_summary.csv", encoding="utf-8-sig").empty

        shroom_out = tmp / "shrooms"
        shroom_out.mkdir()
        shrooms = summarize_shrooms(df, shroom_out)
        assert shrooms[["植物路", "植物列"]].astype(int).to_numpy().tolist() == [[3, 7]]

        cases = build_cases(
            df,
            csv_path,
            tmp,
            {"num_test": 100, "test_type_plant": 0, "row_settings": {"3": {"crush_col": 7, "crush_type": 0}}},
            shrooms,
            None,
        )
        assert cases[0]["meta"] == {"source_csv": str(csv_path), "zombie_wave": 1, "zombie_row": 3}
        assert safe_stem(csv_path) == "damage"

    print("zomboni_replay self-check passed")


if __name__ == "__main__":
    main()
