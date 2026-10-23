"""Exercise the real BSP driver with a GPIO-level bus model; no board access."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
cc = Path("D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
out = root / "build/host-tests/soft_i2c_tests.exe"
out.parent.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(cc), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-Wconversion", "-g", "-O0", "-I" + str(root / "tests/soft_i2c_fakes"),
                "-I" + str(root / "BSP"), str(root / "tests/test_soft_i2c.c"),
                str(root / "BSP/board_soft_i2c.c"), str(root / "BSP/board_time.c"), "-o", str(out)], env=env, check=True)
subprocess.run([str(out)], env=env, check=True)
