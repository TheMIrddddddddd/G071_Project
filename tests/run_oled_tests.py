"""Exercise SH1106 transport against a page/column RAM model."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
cc = Path("D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
out = root / "build/host-tests/oled_tests.exe"
out.parent.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(cc), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-Wconversion", "-g", "-O0", "-I" + str(root / "tests/oled_fakes"),
                "-I" + str(root / "BSP"), "-I" + str(root / "Middleware/OledRefresh"), str(root / "tests/test_oled.c"),
                str(root / "BSP/board_oled.c"), "-o", str(out)], env=env, check=True)
subprocess.run([str(out)], env=env, check=True)
