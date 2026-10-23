"""Run the host framework suites; optional reference comparison needs oled_test."""
from pathlib import Path
import os
import subprocess
import sys
import hashlib

root = Path(__file__).resolve().parents[1]
cmake = "C:/ST/STM32CubeCLT_1.20.0/CMake/bin/cmake.exe"
ctest = "C:/ST/STM32CubeCLT_1.20.0/CMake/bin/ctest.exe"
gcc = Path("D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
ninja = "C:/ST/STM32CubeCLT_1.20.0/Ninja/bin/ninja.exe"
env = dict(os.environ)
env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
def run(args):
    subprocess.run([str(x) for x in args], cwd=root, env=env, check=True)
for suite in ("showcase", "app", "layout"):
    build = root / "build" / (suite + "-tests")
    run([cmake, "-S", root / "tests" / suite, "-B", build, "-G", "Ninja",
         "-DCMAKE_C_COMPILER=" + str(gcc), "-DCMAKE_CXX_COMPILER=" + str(gcc.parent / "g++.exe"),
         "-DCMAKE_MAKE_PROGRAM=" + ninja])
    run([cmake, "--build", build])
    run([ctest, "--test-dir", build, "--output-on-failure"])
exe = root / "build/host-tests/refresh_tests.exe"
exe.parent.mkdir(parents=True, exist_ok=True)
run([gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-I", root / "Middleware/OledRefresh",
     root / "tests/refresh/test_refresh.c", root / "Middleware/OledRefresh/oled_refresh.c", "-o", exe])
run([exe])
for name in ("oled", "soft_i2c", "tca6507", "uart3_log"):
    run([sys.executable, root / "tests" / ("run_" + name + "_tests.py")])
run([sys.executable, root / "scripts/verify_ui_sources.py"])
if "--reference" in sys.argv:
    build = root / "build/reference-tests"
    run([cmake, "-S", root / "tests/reference", "-B", build, "-G", "Ninja",
         "-DCMAKE_C_COMPILER=" + str(gcc), "-DCMAKE_CXX_COMPILER=" + str(gcc.parent / "g++.exe"),
         "-DCMAKE_MAKE_PROGRAM=" + ninja])
    run([cmake, "--build", build])
    old = root / "build/reference-frames.bin"
    new = root / "build/migrated-frames.bin"
    run([build / "test_reference.exe", "preview", old])
    run([root / "build/showcase-tests/test_showcase.exe", "preview", new])
    assert old.read_bytes() == new.read_bytes(), "SHOWCASE pixels differ"
    print("PASS identical 101-second reference frames:", hashlib.sha256(new.read_bytes()).hexdigest())
