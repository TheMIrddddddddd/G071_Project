"""Check real TCA6507 register encoding with a recording I2C transport."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
cc = Path("D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
out = root / "build/host-tests/tca6507_tests.exe"
out.parent.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(cc), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-Wconversion", "-g", "-O0", "-I" + str(root / "BSP"),
                str(root / "tests/test_tca6507.c"),
                str(root / "BSP/board_tca6507.c"), "-o", str(out)], env=env, check=True)
subprocess.run([str(out)], env=env, check=True)
