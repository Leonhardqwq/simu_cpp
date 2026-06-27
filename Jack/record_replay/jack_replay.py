"""Build and run Jack replay cases from damage-recorder CSV files.

The script lives in simu_cpp and treats IO-Records as read-only input.
It accepts an absolute CSV file or directory, then writes editable profiles,
generated Jack case JSON, and optional simulation results under an output directory.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

import pandas as pd


ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def is_frozen() -> bool:
    return bool(getattr(sys, "frozen", False))


def app_dir() -> Path:
    return Path(sys.executable).resolve().parent if is_frozen() else HERE


def resource_dir() -> Path:
    return Path(sys._MEIPASS) if is_frozen() else HERE


def default_base_config() -> Path:
    return resource_dir() / "Jack" / "jack_config.json" if is_frozen() else ROOT / "Jack" / "jack_config.json"


def default_runner_path() -> Path:
    return resource_dir() / "Jack_record_replay.exe" if is_frozen() else HERE / "Jack_record_replay.exe"


JACK_GROUP_COLUMNS = ["zombie_wave", "zombie_row", "zombie_id"]

JACK = "小丑"
ICE_MELON = "冰瓜"
ICE_SHROOM = "寒冰菇"
COB_CANNON = "玉米加农炮"
COB_PROJECTILE = "玉米炮"
ASH_RANGE = [-1000, 1000]
SHROOM_TYPES = {"忧郁菇": 0, "大喷菇": 1}

SCENARIO_DEFAULTS = {
    "num_test": 10000,
    "scene": 1,
    "vulnerable_time": 0,
    "test_type_zombie": 0,
    "test_type_plant": 0,
}
SCENARIO_KEYS = list(SCENARIO_DEFAULTS)
FORCE_DEFAULT_SCENARIO_KEYS = {
    "num_test",
    "test_type_zombie",
    "test_type_plant",
    "vulnerable_time",
}
ROW_DEFAULTS = {
    "defense_type": 0,
    "jack_type": 0,
    "ash_infos": [],
}

SHROOM_COLUMNS = ["启用", "永动", "植物路", "植物列", "植物类型", "备注"]
SHROOM_KEY_COLUMNS = ["植物类型", "植物路", "植物列"]
SHROOM_EDITABLE_COLUMNS = ["启用", "永动", "备注"]

RESULT_COLUMNS = ["炸率", "error"]
IGNORE_COLUMN = "可忽略"
SUMMARY_KEY_COLUMNS = ["僵尸波次", "僵尸路", "僵尸ID"]
SUMMARY_COLUMNS = RESULT_COLUMNS + [IGNORE_COLUMN] + SUMMARY_KEY_COLUMNS + [
    "最后伤害时机",
    "冰时机",
    "灰烬信息",
    "额外伤害",
    "冰瓜伤害",
    "溅射信息",
    "警告数量",
]


def safe_stem(path: Path) -> str:
    return re.sub(r'[<>:"/\\|?*\s]+', "_", path.stem).strip("_") or "damage_csv"


def damage_input(args: argparse.Namespace) -> Path:
    path = Path(args.csv_path)
    if not path.is_absolute():
        raise SystemExit(f"CSV path must be absolute: {args.csv_path}")
    if not path.exists():
        raise SystemExit(f"CSV path does not exist: {path}")
    return path


def input_csvs(path: Path) -> list[Path]:
    csvs = [path] if path.is_file() else sorted(path.glob("*.csv"))
    if not csvs:
        raise SystemExit(f"No CSV files found: {path}")
    return csvs


def default_out_dir(path: Path) -> Path:
    return app_dir() / "out" / safe_stem(path)


def replay_paths(args: argparse.Namespace) -> tuple[Path, Path]:
    csv_path = damage_input(args)
    if not csv_path.is_file():
        raise SystemExit(f"CSV path must be a file for single-file mode: {csv_path}")
    if args.out:
        out_dir = Path(args.out).resolve()
    else:
        out_dir = default_out_dir(csv_path)
    return csv_path, out_dir


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: Any) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def read_damage_csv(csv_path: Path) -> pd.DataFrame:
    return pd.read_csv(
        csv_path,
        encoding="utf-8-sig",
        header=0,
        names=[
            "wave",
            "time",
            "absolute_time",
            "damage",
            "zombie_id",
            "zombie_type",
            "zombie_row",
            "zombie_x",
            "zombie_wave",
            "zombie_total_hp",
            "zombie_below_critical",
            "bullet_id",
            "projectile_type",
            "plant_id",
            "plant_type",
            "plant_row",
            "plant_col",
        ],
    )


def wave_lengths(df: pd.DataFrame) -> dict[int, int]:
    starts = (df["absolute_time"].astype(int) - df["time"].astype(int)).groupby(df["wave"].astype(int)).min()
    return {
        int(wave): max(int(starts[wave + 1] - start), 601)
        for wave, start in starts.items()
        if wave + 1 in starts
    }


def replay_time(record: Any, zombie_wave: int, wave_lengths: dict[int, int]) -> int | None:
    damage_wave = int(record.wave)
    if damage_wave < zombie_wave:
        return None
    waves = range(zombie_wave, damage_wave)
    if any(wave not in wave_lengths for wave in waves):
        return None
    return int(record.time) + sum(wave_lengths[wave] for wave in waves)


def default_row_settings(zombie_row: int, shrooms: pd.DataFrame) -> dict[str, Any]:
    candidates = shrooms[
        shrooms["启用"].eq(1) & shrooms["植物路"].between(zombie_row - 1, zombie_row + 1)
    ].assign(_priority=lambda plant: plant["植物路"].map({zombie_row: 0, zombie_row - 1: 1, zombie_row + 1: 2}))
    target = candidates.sort_values(["植物列", "_priority"], ascending=[False, True]).iloc[0]
    relative_row = int(target["植物路"]) - zombie_row
    return {
        "boom_type": {0: 2, -1: 1, 1: 0}[relative_row],
        "boom_col": int(target["植物列"]),
        **ROW_DEFAULTS,
    }


def load_or_create_scenario(
    out_dir: Path,
    base_config: Path,
    num_test: int | None,
    rows: list[int],
    shrooms: pd.DataFrame,
) -> dict[str, Any]:
    path = out_dir / "scenario_profile.json"
    if path.exists():
        scenario = read_json(path)
        saved_rows = scenario["row_settings"]
    else:
        base = read_json(base_config)
        scenario = {
            key: default if key in FORCE_DEFAULT_SCENARIO_KEYS else base[key]
            for key, default in SCENARIO_DEFAULTS.items()
        }
        saved_rows = {}

    if num_test is not None:
        scenario["num_test"] = int(num_test)
    scenario = {key: scenario[key] for key in SCENARIO_KEYS}
    scenario["row_settings"] = {
        str(row): saved_rows[str(row)] if str(row) in saved_rows else default_row_settings(row, shrooms)
        for row in rows
    }
    write_json(path, scenario)
    return scenario


def summarize_shrooms(df: pd.DataFrame, out_dir: Path) -> pd.DataFrame:
    path = out_dir / "shroom_profile.csv"
    mask = df["plant_type"].astype(str).isin(SHROOM_TYPES)
    fresh = df.loc[mask, ["plant_type", "plant_row", "plant_col"]].drop_duplicates()
    fresh.columns = ["植物类型", "植物路", "植物列"]
    fresh = fresh.assign(**{"启用": 1, "永动": 1, "备注": ""})[SHROOM_COLUMNS]

    if path.exists():
        old = pd.read_csv(path, encoding="utf-8-sig").set_index(SHROOM_KEY_COLUMNS).sort_index()
        fresh = fresh.set_index(SHROOM_KEY_COLUMNS).sort_index()
        fresh.update(old[SHROOM_EDITABLE_COLUMNS])
        fresh = fresh.reset_index()[SHROOM_COLUMNS]

    fresh = fresh.sort_values(["植物路", "植物列", "植物类型"])
    fresh.to_csv(path, index=False, encoding="utf-8-sig")
    return fresh


def plant_list_for_row(shrooms: pd.DataFrame, zombie_row: int) -> list[list[int]]:
    plants: list[list[int]] = []
    for _, row in shrooms.iterrows():
        if int(row["启用"]) == 0:
            continue
        cpp_type = SHROOM_TYPES[str(row["植物类型"])]
        plant_row = int(row["植物路"])
        if (cpp_type == 1 and plant_row != zombie_row) or (
            cpp_type == 0 and abs(plant_row - zombie_row) > 1
        ):
            continue
        plants.append([int(row["植物列"]), cpp_type, int(row["永动"])])
    return sorted(plants)


def merge_splash_infos(items: list[tuple[int, int]]) -> list[list[int]]:
    merged: dict[int, int] = {}
    for time, damage in items:
        merged[time] = merged.get(time, 0) + damage
    return [[time, merged[time]] for time in sorted(merged)]


def ash_info(time: int, plant_type: str, projectile_type: str) -> tuple[int, int, int, int]:
    return (time, 0 if projectile_type == COB_PROJECTILE or plant_type == COB_CANNON else 1, ASH_RANGE[0], ASH_RANGE[1])


def jack_groups(df: pd.DataFrame):
    return df[df["zombie_type"].astype(str).eq(JACK)].groupby(JACK_GROUP_COLUMNS, sort=True)


def jack_rows(df: pd.DataFrame) -> list[int]:
    return sorted(int(row) for row in df.loc[df["zombie_type"].eq(JACK), "zombie_row"].unique())


def summary_row(key: tuple[Any, ...], **computed: Any) -> dict[str, Any]:
    zombie_wave, zombie_row, zombie_id = map(int, key)
    row = dict.fromkeys(SUMMARY_COLUMNS, "")
    row.update({"僵尸波次": zombie_wave, "僵尸路": zombie_row, "僵尸ID": zombie_id})
    row.update(computed)
    return row


def write_summary(rows: list[dict[str, Any]], out_dir: Path) -> None:
    path = out_dir / "jacks_from_damage.csv"
    summary = preserve_ignored(pd.DataFrame(rows, columns=SUMMARY_COLUMNS), path, SUMMARY_KEY_COLUMNS)
    summary[SUMMARY_COLUMNS].to_csv(path, index=False, encoding="utf-8-sig")


def preserve_ignored(summary: pd.DataFrame, path: Path, keys: list[str]) -> pd.DataFrame:
    summary[IGNORE_COLUMN] = ""
    if not path.exists():
        return summary
    old = pd.read_csv(path, encoding="utf-8-sig")
    if IGNORE_COLUMN not in old:
        return summary
    return summary.drop(columns=IGNORE_COLUMN).merge(
        old[keys + [IGNORE_COLUMN]].drop_duplicates(keys), on=keys, how="left"
    ).fillna({IGNORE_COLUMN: ""})


def write_rate_summary(out_dir: Path) -> Path:
    summary_path = out_dir / "jacks_from_damage.csv"
    if not summary_path.exists():
        raise FileNotFoundError(f"Summary not found; run first: {summary_path}")
    summary = pd.read_csv(summary_path, encoding="utf-8-sig")
    rates = pd.to_numeric(summary["炸率"], errors="coerce")
    ignored = pd.to_numeric(summary[IGNORE_COLUMN], errors="coerce").eq(1)
    valid = summary.loc[rates.notna() & ~ignored, ["僵尸路"]].assign(炸率=rates)
    result = valid.groupby("僵尸路")["炸率"].agg(参与计算数量="count", 平均炸率="mean").reset_index()
    path = out_dir / "jack_rate_summary.csv"
    result.to_csv(path, index=False, encoding="utf-8-sig")
    return path


def case_id_for(csv_path: Path, zombie_wave: int, zombie_row: int, zombie_id: int) -> str:
    stem = re.sub(r"\s+", "_", csv_path.stem)
    return f"{stem}_wave{zombie_wave:02d}_row{zombie_row}_id{zombie_id}"


def build_cases(
    df: pd.DataFrame,
    csv_path: Path,
    out_dir: Path,
    scenario: dict[str, Any],
    shrooms: pd.DataFrame,
    wave_lengths: dict[int, int],
    limit: int | None,
) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    summaries: list[dict[str, Any]] = []

    for key, group in jack_groups(df):
        zombie_wave, zombie_row, zombie_id = map(int, key)
        settings = scenario["row_settings"][str(zombie_row)]
        ice_times: set[int] = set()
        splashes: list[tuple[int, int]] = []
        auto_ashes: set[tuple[int, int, int, int]] = set()
        precritical_melon_damage = 0
        extra_dmg = 0
        last_damage_time: int | None = None
        warning_count = 0

        for record in group.sort_values(["wave", "time"]).itertuples(index=False):
            time = replay_time(record, zombie_wave, wave_lengths)
            if time is None:
                warning_count += 1
                continue
            damage = int(record.damage)
            plant_type = str(record.plant_type)
            projectile_type = str(record.projectile_type)
            if int(record.zombie_below_critical) == 0:
                last_damage_time = time if last_damage_time is None else max(last_damage_time, time)
                if projectile_type == ICE_MELON:
                    precritical_melon_damage += damage
                elif (
                    plant_type != ICE_SHROOM
                    and plant_type not in SHROOM_TYPES
                    and damage < 1800
                ):
                    extra_dmg += damage
            if damage >= 1800:
                auto_ashes.add(ash_info(time, plant_type, projectile_type))
            if plant_type == ICE_SHROOM:
                ice_times.add(time)
            elif projectile_type == ICE_MELON:
                splashes.append((time, damage))

        ice_t = sorted(ice_times)
        splash_infos = merge_splash_infos(splashes)
        ash_infos = [list(info) for info in sorted(auto_ashes)] + settings["ash_infos"]
        config = {key: scenario[key] for key in SCENARIO_KEYS} | {
            "hugewave": zombie_wave in (10, 20),
            "boom_type": settings["boom_type"],
            "boom_col": settings["boom_col"],
            "defense_type": settings["defense_type"],
            "jack_type": settings["jack_type"],
            "ash_infos": ash_infos,
            "ice_t": ice_t,
            "splash_infos": splash_infos,
            "extra_dmg": extra_dmg,
            "plant_list": plant_list_for_row(shrooms, zombie_row),
        }
        event_times = (
            ice_t
            + [time for time, _ in splash_infos]
            + [int(info[0]) for info in config["ash_infos"]]
            + [int(config["vulnerable_time"])]
        )

        cases.append(
            {
                "case_id": case_id_for(csv_path, zombie_wave, zombie_row, zombie_id),
                "meta": {
                    "source_csv": str(csv_path),
                    "zombie_id": zombie_id,
                    "zombie_wave": zombie_wave,
                    "zombie_row": zombie_row,
                },
                "config": config,
                "M_min": max(2600, max(event_times) + 600),
            }
        )
        summaries.append(
            summary_row(
                key,
                **{
                    "最后伤害时机": "" if last_damage_time is None else last_damage_time,
                    "冰时机": json.dumps(ice_t, ensure_ascii=False),
                    "灰烬信息": json.dumps(ash_infos, ensure_ascii=False),
                    "额外伤害": extra_dmg,
                    "冰瓜伤害": precritical_melon_damage,
                    "溅射信息": json.dumps(splash_infos, ensure_ascii=False),
                    "警告数量": warning_count,
                },
            )
        )
        if limit is not None and len(cases) >= limit:
            break

    write_summary(summaries, out_dir)
    write_rate_summary(out_dir)
    write_json(
        out_dir / "jack_batch.json",
        {
            "source_csv": str(csv_path),
            "generated_by": str(Path(__file__).resolve()),
            "cases": cases,
        },
    )
    return cases


def merge_results_into_summary(out_dir: Path, results_path: Path) -> Path:
    summary_path = out_dir / "jacks_from_damage.csv"
    summary = pd.read_csv(summary_path, encoding="utf-8-sig")
    results = pd.read_csv(results_path, encoding="utf-8-sig")
    summary = summary.set_index(SUMMARY_KEY_COLUMNS)
    results = results.set_index(SUMMARY_KEY_COLUMNS)
    summary.update(results[RESULT_COLUMNS])
    summary = summary.reset_index()

    summary = summary[SUMMARY_COLUMNS]
    summary.to_csv(summary_path, index=False, encoding="utf-8-sig")
    write_rate_summary(out_dir)
    return summary_path


def build_one(
    args: argparse.Namespace,
    csv_path: Path,
    out_dir: Path,
    scenario: dict[str, Any] | None = None,
    shrooms: pd.DataFrame | None = None,
) -> list[dict[str, Any]]:
    out_dir.mkdir(parents=True, exist_ok=True)
    df = read_damage_csv(csv_path)
    shrooms = shrooms if shrooms is not None else summarize_shrooms(df, out_dir)
    scenario = scenario or load_or_create_scenario(
        out_dir, Path(args.base_config).resolve(), args.num_test, jack_rows(df), shrooms
    )
    cases = build_cases(
        df, csv_path, out_dir, scenario, shrooms, wave_lengths(df), args.limit
    )
    print(f"CSV: {csv_path}")
    print(f"Output: {out_dir}")
    print(f"Built cases: {len(cases)}")
    print(f"Summary: {out_dir / 'jacks_from_damage.csv'}")
    print(f"Batch: {out_dir / 'jack_batch.json'}")
    return cases


def build_many(args: argparse.Namespace, input_dir: Path, out_dir: Path) -> tuple[Path, list[dict[str, Any]]]:
    csvs = input_csvs(input_dir)
    frames = [read_damage_csv(csv_path) for csv_path in csvs]
    all_damage = pd.concat(frames, ignore_index=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    shrooms = summarize_shrooms(all_damage, out_dir)
    rows = sorted({row for frame in frames for row in jack_rows(frame)})
    scenario = load_or_create_scenario(
        out_dir, Path(args.base_config).resolve(), args.num_test, rows, shrooms
    )

    cases: list[dict[str, Any]] = []
    print(f"Input directory: {input_dir}")
    print(f"Output: {out_dir}")
    print(f"Editable scenario: {out_dir / 'scenario_profile.json'}")
    print(f"Editable shrooms: {out_dir / 'shroom_profile.csv'}")
    for csv_path, frame in zip(csvs, frames):
        child_out = out_dir / safe_stem(csv_path)
        child_out.mkdir(parents=True, exist_ok=True)
        child_cases = build_cases(
            frame, csv_path, child_out, scenario, shrooms, wave_lengths(frame), args.limit
        )
        cases.extend(child_cases)
        print(f"Built {len(child_cases)} cases: {csv_path.name} -> {child_out}")
    write_group_summary(csvs, input_dir, out_dir)
    print("Next: edit scenario/shrooms if needed, then run.")
    return out_dir, cases


def write_group_summary(csvs: list[Path], input_dir: Path, out_dir: Path) -> None:
    frames = []
    for csv_path in csvs:
        df = pd.read_csv(out_dir / safe_stem(csv_path) / "jacks_from_damage.csv", encoding="utf-8-sig")
        df["来源文件"] = csv_path.relative_to(input_dir).as_posix()
        frames.append(df)
    path = out_dir / "jacks_from_damage.csv"
    summary = preserve_ignored(
        pd.concat(frames, ignore_index=True), path, ["来源文件"] + SUMMARY_KEY_COLUMNS
    )
    summary[SUMMARY_COLUMNS + ["来源文件"]].to_csv(path, index=False, encoding="utf-8-sig")
    write_rate_summary(out_dir)


def build_from_profiles(args: argparse.Namespace) -> tuple[Path, list[dict[str, Any]]]:
    path = damage_input(args)
    if path.is_dir():
        return build_many(args, path, Path(args.out).resolve() if args.out else default_out_dir(path))

    csv_path, out_dir = replay_paths(args)
    cases = build_one(args, csv_path, out_dir)
    print(f"Editable scenario: {out_dir / 'scenario_profile.json'}")
    print(f"Editable shrooms: {out_dir / 'shroom_profile.csv'}")
    print("Next: edit scenario/shrooms if needed, then run.")
    return out_dir, cases


def build_runner(args: argparse.Namespace) -> Path:
    source = HERE / "Jack_record_replay.cpp"
    executable = Path(args.exe).resolve() if args.exe else source.with_suffix(".exe")
    subprocess.run(
        [args.cxx, "-std=c++17", "-O2", str(source), "-o", str(executable)], cwd=ROOT, check=True
    )
    return executable


def run_batch(args: argparse.Namespace, out_dir: Path | None = None) -> Path:
    if out_dir is None:
        _, out_dir = replay_paths(args)
    batch_path = out_dir / "jack_batch.json"
    results_path = out_dir / ".jack_results.tmp.csv"
    single_case_path = out_dir / ".jack_single_case.tmp.json"
    executable = Path(args.exe).resolve() if args.exe else default_runner_path()
    if (args.build or not executable.exists()) and not is_frozen():
        executable = build_runner(args)
    if not executable.exists():
        raise FileNotFoundError(f"Jack_record_replay runner not found: {executable}")

    cases = read_json(batch_path)["cases"]
    summary_path = out_dir / "jacks_from_damage.csv"
    try:
        for index, case in enumerate(cases, start=1):
            write_json(single_case_path, {"cases": [case]})
            subprocess.run([str(executable), str(single_case_path), str(results_path)], cwd=ROOT, check=True)
            summary_path = merge_results_into_summary(out_dir, results_path)
            print(f"Updated summary after case {index}/{len(cases)}: {case['case_id']}")
    finally:
        results_path.unlink(missing_ok=True)
        single_case_path.unlink(missing_ok=True)

    print(f"Summary/results: {summary_path}")
    return summary_path


def run_from_profiles(args: argparse.Namespace) -> None:
    path = damage_input(args)
    out_dir, _ = build_from_profiles(args)
    if path.is_file():
        run_batch(args, out_dir)
    else:
        for csv_path in input_csvs(path):
            run_batch(args, out_dir / safe_stem(csv_path))
        write_group_summary(input_csvs(path), path, out_dir)


def calculate_rates(args: argparse.Namespace) -> None:
    path = damage_input(args)
    out_dir = Path(args.out).resolve() if args.out else default_out_dir(path)
    targets = [out_dir] if path.is_file() else [out_dir / safe_stem(csv) for csv in input_csvs(path)] + [out_dir]
    for target in targets:
        print(f"Rate summary: {write_rate_summary(target)}")


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("csv_path", help="absolute path to a damage-recorder CSV file or directory")
    parser.add_argument(
        "command", nargs="?", choices=("build", "run", "calc"), default="build",
        help="command to run; defaults to build",
    )
    parser.add_argument("--out", help="output directory; defaults to app/script out/<csv-stem>")
    parser.add_argument(
        "--base-config",
        default=str(default_base_config()),
        help="base Jack config used only when scenario_profile.json is first created",
    )
    parser.add_argument("--num-test", type=int, help="override scenario_profile.json num_test for this generation")
    parser.add_argument("--limit", type=int, help="generate only the first N jack cases, useful for smoke tests")
    parser.add_argument("--cxx", default="g++", help="C++ compiler used by run when building the replay runner")
    parser.add_argument("--exe", help="path to Jack_record_replay executable")
    parser.add_argument("--build", action="store_true", help="force rebuild of the replay runner before running")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    add_common_args(parser)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.command == "run":
        run_from_profiles(args)
    elif args.command == "calc":
        calculate_rates(args)
    else:
        build_from_profiles(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
