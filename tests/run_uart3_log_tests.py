"""Compile the real UART3 logger against a fake HAL and run host tests."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=shutil.which("gcc") or
                    "D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
args = parser.parse_args()
compiler = Path(args.cc)
if not compiler.is_file():
    parser.error("Native GCC not found; provide --cc with a host compiler path")
output = root / "build" / "host-tests"
output.mkdir(parents=True, exist_ok=True)
exe = output / ("uart3_log_tests.exe" if os.name == "nt" else "uart3_log_tests")
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
common = [str(compiler), "-std=c11", "-Wall", "-Wextra", "-Werror",
          "-Wformat=2", "-g", "-O0", "-isystem",
          str(root / "Middleware" / "Ringbuffer" / "inc")]
# Preserve the imported middleware's existing integer/bitfield conventions.
# BSP and test code additionally use -Wconversion.
ring_object = output / "ringbuffer.o"
subprocess.run(common + ["-c",
                str(root / "Middleware" / "Ringbuffer" / "src" / "ringbuffer.c"),
                "-o", str(ring_object)], cwd=root, env=env, check=True)
subprocess.run(common + ["-Wconversion",
                "-I" + str(root / "tests" / "fakes"),
                "-I" + str(root / "Core" / "Inc"),
                "-I" + str(root / "BSP"),
                str(root / "BSP" / "board_usart.c"),
                str(ring_object),
                str(root / "tests" / "test_uart3_log.c"), "-o", str(exe)],
               cwd=root, env=env, check=True)
subprocess.run([str(exe)], cwd=root, env=env, check=True)
