"""Check an ARM ELF's Flash/RAM budget and linked stack reservation."""
from pathlib import Path
import subprocess
import sys

nm = "C:/ST/STM32CubeCLT_1.20.0/GNU-tools-for-STM32/bin/arm-none-eabi-nm.exe"
path = Path(sys.argv[1])
symbols = {}
for line in subprocess.check_output([nm, str(path)], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        try:
            symbols[fields[2]] = int(fields[0], 16)
        except ValueError:
            pass
ram = symbols["_end"] - 0x20000000 + symbols["_Min_Heap_Size"] + symbols["_Min_Stack_Size"]
assert ram <= 32768, ram
assert symbols["_Min_Stack_Size"] >= 4096
flash = symbols["_sidata"] + symbols["_edata"] - symbols["_sdata"] - 0x08000000
assert flash <= 131072, flash
print(f"PASS: {path.parent.name} Flash={flash} <=131072; reserved RAM={ram} <=32768; stack>=4096")
