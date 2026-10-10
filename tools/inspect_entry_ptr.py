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

e0 = 0x7FF4FDDF4E00
p = r64(e0)
print(f"Pointer at +0x00 of Entry 0: 0x{p:X}")
print("Bytes at 0x7FF7BAA78900:", rstr(p, 64))

# Let's check string at p + offsets:
for o in [0, 8, 16, 24]:
    print(f"  +0x{o:X}: {rstr(p + o, 32)}")

kernel32.CloseHandle(handle)
