import ctypes
from ctypes import wintypes
import subprocess

kernel32 = ctypes.windll.kernel32
cmd = 'powershell -Command "(Get-Process -Name OLGame).Id"'
res = subprocess.check_output(cmd, shell=True).decode().strip()
pid = int(res.split()[0])
handle = kernel32.OpenProcess(0x1F0FFF, False, pid)

def r64(addr):
    buf = ctypes.c_uint64()
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), ctypes.byref(buf), 8, ctypes.byref(read))
    return buf.value

def rstr(addr, max_len=64):
    buf = ctypes.create_string_buffer(max_len)
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, max_len, ctypes.byref(read))
    return buf.raw

gnames_table = 0x7FF4CB570040
for i in [0, 1, 2, 3, 4, 5]:
    entry = r64(gnames_table + i * 8)
    raw = rstr(entry, 40)
    print(f"Index {i} (0x{entry:X}):\n  Hex: {raw.hex()}\n  Raw: {raw}")

kernel32.CloseHandle(handle)
