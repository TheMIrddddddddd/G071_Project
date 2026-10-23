"""Build/run status-bar animation tests and export real UI frames."""
from pathlib import Path
import os
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
cmake = "C:/ST/STM32CubeCLT_1.20.0/CMake/bin/cmake.exe"
ctest = "C:/ST/STM32CubeCLT_1.20.0/CMake/bin/ctest.exe"
gcc = Path("D:/QT/QT/Tools/mingw730_64/bin/gcc.exe")
ninja = "C:/ST/STM32CubeCLT_1.20.0/Ninja/bin/ninja.exe"
build = root / "build/layout-tests"
env = dict(os.environ)
env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
def run(args):
    subprocess.run([str(a) for a in args], cwd=root, env=env, check=True)
run([cmake, "-S", root / "tests/layout", "-B", build, "-G", "Ninja",
     "-DCMAKE_C_COMPILER=" + str(gcc), "-DCMAKE_CXX_COMPILER=" + str(gcc.parent / "g++.exe"),
     "-DCMAKE_MAKE_PROGRAM=" + ninja])
run([cmake, "--build", build])
run([ctest, "--test-dir", build, "--output-on-failure"])
if len(sys.argv) == 2:
    run([build / "test_layout.exe", sys.argv[1]])
