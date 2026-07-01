import json
import re
from pathlib import Path
from typing import Any

import pandas as pd


CSV_COLUMNS = [
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
]

SHROOM_TYPES = {"忧郁菇": 0, "大喷菇": 1}
SHROOM_COLUMNS = ["启用", "永动", "植物路", "植物列", "植物类型", "备注"]
SHROOM_KEY_COLUMNS = ["植物类型", "植物路", "植物列"]
SHROOM_EDITABLE_COLUMNS = ["启用", "永动", "备注"]
IGNORE_COLUMN = "可忽略"
SOURCE_COLUMN = "来源文件"


def safe_stem(path: Path) -> str:
    return re.sub(r'[<>:"/\\|?*\s]+', "_", path.stem).strip("_") or "damage_csv"


def damage_input(path_text: str) -> Path:
    path = Path(path_text)
    if not path.is_absolute():
        raise SystemExit(f"CSV path must be absolute: {path_text}")
    if not path.exists():
        raise SystemExit(f"CSV path does not exist: {path}")
    return path


def input_csvs(path: Path) -> list[Path]:
    csvs = [path] if path.is_file() else sorted(path.glob("*.csv"))
    if not csvs:
        raise SystemExit(f"No CSV files found: {path}")
    return csvs


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: Any) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def read_damage_csv(csv_path: Path) -> pd.DataFrame:
    return pd.read_csv(csv_path, encoding="utf-8-sig", header=0, names=CSV_COLUMNS)


def summarize_shrooms(df: pd.DataFrame, out_dir: Path) -> pd.DataFrame:
    path = out_dir / "shroom_profile.csv"
    fresh = df.loc[
        df["plant_type"].astype(str).isin(SHROOM_TYPES),
        ["plant_type", "plant_row", "plant_col"],
    ].drop_duplicates()
    fresh.columns = ["植物类型", "植物路", "植物列"]
    fresh = fresh.assign(**{"启用": 1, "永动": 1, "备注": ""})[SHROOM_COLUMNS]

    if path.exists():
        old = pd.read_csv(path, encoding="utf-8-sig").set_index(SHROOM_KEY_COLUMNS).sort_index()
        fresh = fresh.set_index(SHROOM_KEY_COLUMNS).sort_index()
        fresh.update(old[SHROOM_EDITABLE_COLUMNS])
        fresh = fresh.reset_index()[SHROOM_COLUMNS]

    fresh[SHROOM_KEY_COLUMNS[1:]] = fresh[SHROOM_KEY_COLUMNS[1:]].astype(int)
    fresh = fresh.sort_values(["植物路", "植物列", "植物类型"])
    fresh.to_csv(path, index=False, encoding="utf-8-sig")
    return fresh


def shroom_list_for_row(shrooms: pd.DataFrame, zombie_row: int) -> list[list[int]]:
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


def write_rate_summary(
    out_dir: Path,
    summary_filename: str,
    output_filename: str,
    rate_column: str,
    row_column: str,
    count_column: str,
    mean_column: str,
) -> Path:
    summary_path = out_dir / summary_filename
    if not summary_path.exists():
        raise FileNotFoundError(f"Summary not found; run first: {summary_path}")
    summary = pd.read_csv(summary_path, encoding="utf-8-sig")
    rates = pd.to_numeric(summary[rate_column], errors="coerce")
    ignored = pd.to_numeric(summary[IGNORE_COLUMN], errors="coerce").eq(1)
    valid = summary.loc[rates.notna() & ~ignored, [row_column]].assign(**{rate_column: rates})
    result = valid.groupby(row_column)[rate_column].agg(**{count_column: "count", mean_column: "mean"}).reset_index()
    path = out_dir / output_filename
    result.to_csv(path, index=False, encoding="utf-8-sig")
    return path
