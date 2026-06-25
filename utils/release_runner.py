from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from types import ModuleType


def app_dir(dev_dir: Path) -> Path:
    return Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else dev_dir


def runner_path(dev_dir: Path, runner_name: str) -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS) / runner_name
    return dev_dir / runner_name


def run_simulator(read_info: ModuleType, dev_dir: Path, runner_name: str) -> int:
    work_dir = app_dir(dev_dir)
    excel_path = work_dir / read_info.file_name
    json_path = work_dir / read_info.config_name
    result_path = work_dir / "result.txt"
    cpp_runner = runner_path(dev_dir, runner_name)

    with result_path.open("w", encoding="utf-8") as log:
        def emit(text: str = "") -> None:
            print(text)
            log.write(text + "\n")
            log.flush()

        try:
            if not excel_path.exists():
                raise FileNotFoundError(f"找不到 Excel：{excel_path.name}")
            if not cpp_runner.exists():
                raise FileNotFoundError(f"找不到内置模拟器：{cpp_runner.name}")

            emit(f"读取 Excel：{excel_path.name}")
            read_info.write_config(read_info.build_config(excel_path), json_path)
            emit(f"生成配置：{json_path.name}")
            emit("开始模拟...\n")

            proc = subprocess.Popen(
                [str(cpp_runner)],
                cwd=work_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            assert proc.stdout is not None
            for line in proc.stdout:
                print(line, end="")
                log.write(line)
                log.flush()
            return proc.wait()
        except PermissionError as exc:
            emit(f"文件被占用或没有权限：{exc}")
            return 1
        except Exception as exc:
            emit(f"运行失败：{exc}")
            return 1
        finally:
            try:
                input("\n按 Enter 退出...")
            except EOFError:
                pass
