import ctypes
from ctypes import wintypes
import subprocess

kernel32 = ctypes.windll.kernel32
cmd = 'powershell -Command "(Get-Process -Name OLGame).Id"'
res = subprocess.check_output(cmd, shell=True).decode().strip()
pid = int(res.split()[0])
handle = kernel32.OpenProcess(0x1F0FFF, False, pid)

def r32(addr):
    buf = ctypes.c_uint32()
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), ctypes.byref(buf), 4, ctypes.byref(read))
    return buf.value

def rf(addr):
    buf = ctypes.c_float()
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), ctypes.byref(buf), 4, ctypes.byref(read))
    return buf.value

cam = 0x7FF4CE3F0040
print("Testing rotation changes in 0x7FF4CE3F0040:")
print(f"Pos (+0x80): ({rf(cam+0x80):.1f}, {rf(cam+0x84):.1f}, {rf(cam+0x88):.1f})")
print(f"Rot (+0x8C): ({r32(cam+0x8C)}, {r32(cam+0x90)}, {r32(cam+0x94)})")

# Search for any other locations or rotations in this Camera actor:
for off in range(0x120, 0x500, 4):
    f1 = rf(cam + off)
    f2 = rf(cam + off + 4)
    f3 = rf(cam + off + 8)
    if abs(f1 - rf(cam+0x80)) < 1.0 and abs(f2 - rf(cam+0x84)) < 1.0:
        print(f"  Exact Match Pos at +0x{off:X}!")
        print(f"    Rot after Pos: ({r32(cam+off+12)}, {r32(cam+off+16)}, {r32(cam+off+20)})")

kernel32.CloseHandle(handle)
