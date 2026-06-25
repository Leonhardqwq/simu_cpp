import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

import read_info
sys.path.insert(0, str(ROOT))
from utils.release_runner import run_simulator


if __name__ == "__main__":
    raise SystemExit(run_simulator(read_info, ROOT / "Enter", "Enter_cpp_1_0.exe"))
