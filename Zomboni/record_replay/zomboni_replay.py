"""Build and run Zomboni replay cases from damage-recorder CSV files."""

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
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from utils.record_replay_common import (
    IGNORE_COLUMN,
    SHROOM_COLUMNS,
    SOURCE_COLUMN,
    damage_input as checked_damage_input,
    input_csvs,
    preserve_ignored,
    read_damage_csv,
    read_json,
    safe_stem,
    shroom_list_for_row,
    summarize_shrooms,
    write_json,
    write_rate_summary as common_write_rate_summary,
)


ZOMBONI = "冰车"
ICE_MELON = "冰瓜"
SPIKE_TYPES = {"地刺", "地刺王"}
ZOMBONI_UNIT_COLUMNS = ["zombie_wave", "zombie_row", "zombie_id"]
SUMMARY_KEY_COLUMNS = ["僵尸波次", "僵尸路"]
RESULT_COLUMNS = ["碾率", "error"]
MELON_INFO_COLUMN = "冰瓜信息[冰瓜行,冰瓜列,最小溅射空间,最大溅射空间]"
SUMMARY_COLUMNS = RESULT_COLUMNS + [IGNORE_COLUMN] + SUMMARY_KEY_COLUMNS + [MELON_INFO_COLUMN]
SCENARIO_DEFAULTS = {"num_test": 10000, "test_type_plant": 0}
SCENARIO_KEYS = list(SCENARIO_DEFAULTS)


def is_frozen() -> bool:
    return bool(getattr(sys, "frozen", False))


def app_dir() -> Path:
    return Path(sys.executable).resolve().parent if is_frozen() else HERE


def resource_dir() -> Path:
    return Path(sys._MEIPASS) if is_frozen() else HERE


def default_base_config() -> Path:
    return resource_dir() / "Zomboni" / "zomboni_config.json" if is_frozen() else ROOT / "Zomboni" / "zomboni_config.json"


def default_runner_path() -> Path:
    return resource_dir() / "Zomboni_record_replay.exe" if is_frozen() else HERE / "Zomboni_record_replay.exe"


def damage_input(args: argparse.Namespace) -> Path:
    return checked_damage_input(args.csv_path)


def default_out_dir(path: Path) -> Path:
    return app_dir() / "out" / safe_stem(path)


def replay_paths(args: argparse.Namespace) -> tuple[Path, Path]:
    csv_path = damage_input(args)
    if not csv_path.is_file():
        raise SystemExit(f"CSV path must be a file for single-file mode: {csv_path}")
    return csv_path, Path(args.out).resolve() if args.out else default_out_dir(csv_path)


def valid_zomboni_damage(df: pd.DataFrame) -> pd.DataFrame:
    kept = []
    zdf = df[df["zombie_type"].astype(str).eq(ZOMBONI)]
    for _, group in zdf.groupby(ZOMBONI_UNIT_COLUMNS, sort=True):
        killed = group["damage"].astype(int).ge(1800).any()
        spiked = group["plant_type"].astype(str).isin(SPIKE_TYPES).any()
        if not killed and not spiked:
            kept.append(group)
    return pd.concat(kept, ignore_index=True) if kept else zdf.iloc[0:0].copy()


def zomboni_rows(df: pd.DataFrame) -> list[int]:
    valid = valid_zomboni_damage(df)
    return sorted(int(row) for row in valid["zombie_row"].unique())


def melon_cols_by_row(df: pd.DataFrame) -> dict[int, set[int]]:
    melons: dict[int, set[int]] = {}
    hits = df.loc[df["projectile_type"].astype(str).eq(ICE_MELON), ["plant_row", "plant_col"]].dropna()
    for row in hits.itertuples(index=False):
        melons.setdefault(int(row.plant_row), set()).add(int(row.plant_col))
    return melons


def default_row_settings(row: int, shrooms: pd.DataFrame, melons: dict[int, set[int]], base_col: int) -> dict[str, int]:
    shroom_cols = shrooms.loc[
        shrooms["启用"].eq(1) & shrooms["植物路"].eq(row), "植物列"
    ].astype(int).tolist()
    cols = shroom_cols + list(melons.get(row, set()))
    return {"crush_col": max(cols) if cols else base_col, "crush_type": 0}


def load_or_create_scenario(
    out_dir: Path,
    base_config: Path,
    num_test: int | None,
    rows: list[int],
    shrooms: pd.DataFrame,
    melons: dict[int, set[int]],
) -> dict[str, Any]:
    path = out_dir / "scenario_profile.json"
    if path.exists():
        scenario = read_json(path)
        saved_rows = scenario["row_settings"]
    else:
        scenario = dict(SCENARIO_DEFAULTS)
        saved_rows = {}

    if num_test is not None:
        scenario["num_test"] = int(num_test)
    base_col = int(read_json(base_config)["crush_col"])
    scenario = {key: scenario[key] for key in SCENARIO_KEYS}
    scenario["row_settings"] = {
        str(row): saved_rows[str(row)] if str(row) in saved_rows else default_row_settings(row, shrooms, melons, base_col)
        for row in rows
    }
    write_json(path, scenario)
    return scenario


def select_melon_space(values: list[int]) -> int:
    return min(values)


def melon_info_for_group(group: pd.DataFrame, crush_col: int, crush_type: int) -> tuple[list[list[int]], list[list[int]]]:
    spaces: dict[tuple[int, int], list[int]] = {}
    hits = group.loc[group["projectile_type"].astype(str).eq(ICE_MELON)].dropna(subset=["plant_row", "plant_col"])
    first_hits = hits.sort_values(["zombie_id", "plant_row", "plant_col", "absolute_time", "time"]).groupby(
        ["zombie_id", "plant_row", "plant_col"], sort=True
    ).first().reset_index()

    for hit in first_hits.itertuples(index=False):
        key = (int(hit.plant_row), int(hit.plant_col))
        space = int(float(hit.zombie_x)) - (80 * crush_col + (40 if crush_type != 2 else -40))
        spaces.setdefault(key, []).append(space)

    infos = [[row, col, min(values), max(values)] for (row, col), values in sorted(spaces.items())]
    melon_list = [[select_melon_space(values), 2, 1] for _, values in sorted(spaces.items())]
    return infos, melon_list


def case_id_for(csv_path: Path, zombie_wave: int, zombie_row: int) -> str:
    stem = re.sub(r"\s+", "_", csv_path.stem)
    return f"{stem}_wave{zombie_wave:02d}_row{zombie_row}"


def summary_row(key: tuple[Any, ...], melon_info: list[list[int]]) -> dict[str, Any]:
    zombie_wave, zombie_row = map(int, key)
    row = dict.fromkeys(SUMMARY_COLUMNS, "")
    row.update({"僵尸波次": zombie_wave, "僵尸路": zombie_row, MELON_INFO_COLUMN: json.dumps(melon_info, ensure_ascii=False)})
    return row


def write_summary(rows: list[dict[str, Any]], out_dir: Path) -> None:
    path = out_dir / "zombonis_from_damage.csv"
    summary = preserve_ignored(pd.DataFrame(rows, columns=SUMMARY_COLUMNS), path, SUMMARY_KEY_COLUMNS)
    summary[SUMMARY_COLUMNS].to_csv(path, index=False, encoding="utf-8-sig")


def write_rate_summary(out_dir: Path) -> Path:
    return common_write_rate_summary(
        out_dir,
        "zombonis_from_damage.csv",
        "zomboni_rate_summary.csv",
        "碾率",
        "僵尸路",
        "参与计算波次",
        "平均碾率",
    )


def build_cases(
    df: pd.DataFrame,
    csv_path: Path,
    out_dir: Path,
    scenario: dict[str, Any],
    shrooms: pd.DataFrame,
    limit: int | None,
) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    summaries: list[dict[str, Any]] = []
    valid = valid_zomboni_damage(df)

    for key, group in valid.groupby(["zombie_wave", "zombie_row"], sort=True):
        zombie_wave, zombie_row = map(int, key)
        settings = scenario["row_settings"][str(zombie_row)]
        melon_info, melon_list = melon_info_for_group(
            group, int(settings["crush_col"]), int(settings["crush_type"])
        )
        config = {key: scenario[key] for key in SCENARIO_KEYS} | {
            "show_progress": False,
            "crush_col": int(settings["crush_col"]),
            "crush_type": int(settings["crush_type"]),
            "shroom_list": shroom_list_for_row(shrooms, zombie_row),
            "melon_list": melon_list,
        }
        cases.append(
            {
                "case_id": case_id_for(csv_path, zombie_wave, zombie_row),
                "meta": {
                    "source_csv": str(csv_path),
                    "zombie_wave": zombie_wave,
                    "zombie_row": zombie_row,
                },
                "config": config,
            }
        )
        summaries.append(summary_row(key, melon_info))
        if limit is not None and len(cases) >= limit:
            break

    write_summary(summaries, out_dir)
    write_rate_summary(out_dir)
    write_json(
        out_dir / "zomboni_batch.json",
        {
            "source_csv": str(csv_path),
            "generated_by": str(Path(__file__).resolve()),
            "cases": cases,
        },
    )
    return cases


def merge_results_into_summary(out_dir: Path, results_path: Path) -> Path:
    summary_path = out_dir / "zombonis_from_damage.csv"
    summary = pd.read_csv(summary_path, encoding="utf-8-sig")
    results = pd.read_csv(results_path, encoding="utf-8-sig")
    summary = summary.set_index(SUMMARY_KEY_COLUMNS)
    results = results.set_index(SUMMARY_KEY_COLUMNS)
    summary.update(results[RESULT_COLUMNS])
    summary = summary.reset_index()[SUMMARY_COLUMNS]
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
        out_dir, Path(args.base_config).resolve(), args.num_test, zomboni_rows(df), shrooms, melon_cols_by_row(df)
    )
    cases = build_cases(df, csv_path, out_dir, scenario, shrooms, args.limit)
    print(f"CSV: {csv_path}")
    print(f"Output: {out_dir}")
    print(f"Built cases: {len(cases)}")
    print(f"Summary: {out_dir / 'zombonis_from_damage.csv'}")
    print(f"Batch: {out_dir / 'zomboni_batch.json'}")
    return cases


def build_many(args: argparse.Namespace, input_dir: Path, out_dir: Path) -> tuple[Path, list[dict[str, Any]]]:
    csvs = input_csvs(input_dir)
    frames = [read_damage_csv(csv_path) for csv_path in csvs]
    all_damage = pd.concat(frames, ignore_index=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    shrooms = summarize_shrooms(all_damage, out_dir)
    rows = sorted({row for frame in frames for row in zomboni_rows(frame)})
    scenario = load_or_create_scenario(
        out_dir, Path(args.base_config).resolve(), args.num_test, rows, shrooms, melon_cols_by_row(all_damage)
    )

    cases: list[dict[str, Any]] = []
    print(f"Input directory: {input_dir}")
    print(f"Output: {out_dir}")
    print(f"Editable scenario: {out_dir / 'scenario_profile.json'}")
    print(f"Editable shrooms: {out_dir / 'shroom_profile.csv'}")
    for csv_path, frame in zip(csvs, frames):
        child_out = out_dir / safe_stem(csv_path)
        child_out.mkdir(parents=True, exist_ok=True)
        child_cases = build_cases(frame, csv_path, child_out, scenario, shrooms, args.limit)
        cases.extend(child_cases)
        print(f"Built {len(child_cases)} cases: {csv_path.name} -> {child_out}")
    write_group_summary(csvs, input_dir, out_dir)
    print("Next: edit scenario/shrooms if needed, then run.")
    return out_dir, cases


def write_group_summary(csvs: list[Path], input_dir: Path, out_dir: Path) -> None:
    frames = []
    for csv_path in csvs:
        df = pd.read_csv(out_dir / safe_stem(csv_path) / "zombonis_from_damage.csv", encoding="utf-8-sig")
        df[SOURCE_COLUMN] = csv_path.relative_to(input_dir).as_posix()
        frames.append(df)
    path = out_dir / "zombonis_from_damage.csv"
    summary = preserve_ignored(
        pd.concat(frames, ignore_index=True), path, [SOURCE_COLUMN] + SUMMARY_KEY_COLUMNS
    )
    summary[SUMMARY_COLUMNS + [SOURCE_COLUMN]].to_csv(path, index=False, encoding="utf-8-sig")
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
    source = HERE / "Zomboni_record_replay.cpp"
    executable = Path(args.exe).resolve() if args.exe else source.with_suffix(".exe")
    subprocess.run([args.cxx, "-std=c++17", "-O2", str(source), "-o", str(executable)], cwd=ROOT, check=True)
    return executable


def run_batch(args: argparse.Namespace, out_dir: Path | None = None) -> Path:
    if out_dir is None:
        _, out_dir = replay_paths(args)
    batch_path = out_dir / "zomboni_batch.json"
    results_path = out_dir / ".zomboni_results.tmp.csv"
    single_case_path = out_dir / ".zomboni_single_case.tmp.json"
    executable = Path(args.exe).resolve() if args.exe else default_runner_path()
    if (args.build or not executable.exists()) and not is_frozen():
        executable = build_runner(args)
    if not executable.exists():
        raise FileNotFoundError(f"Zomboni_record_replay runner not found: {executable}")

    cases = read_json(batch_path)["cases"]
    summary_path = out_dir / "zombonis_from_damage.csv"
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
        help="base Zomboni config used only when scenario_profile.json is first created",
    )
    parser.add_argument("--num-test", type=int, help="override scenario_profile.json num_test for this generation")
    parser.add_argument("--limit", type=int, help="generate only the first N zomboni cases, useful for smoke tests")
    parser.add_argument("--cxx", default="g++", help="C++ compiler used by run when building the replay runner")
    parser.add_argument("--exe", help="path to Zomboni_record_replay executable")
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
