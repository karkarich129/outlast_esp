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
print(f"Investigating Camera Actor 0x{cam:X}:")
for off in range(0x200, 0x300, 4):
    f = rf(cam + off)
    i = r32(cam + off)
    # Check if 3 consecutive floats look like position (e.g. 1424.8, 1668.3, -351.5)
    f1 = rf(cam + off)
    f2 = rf(cam + off + 4)
    f3 = rf(cam + off + 8)
    if abs(f1) > 10.0 and abs(f2) > 10.0 and abs(f3) > 10.0 and abs(f1) < 1000000.0:
        rot1 = r32(cam + off + 12)
        rot2 = r32(cam + off + 16)
        rot3 = r32(cam + off + 20)
        fov = rf(cam + off + 24)
        print(f"  Camera Candidate at +0x{off:X}: Pos:({f1:.1f}, {f2:.1f}, {f3:.1f}) | Rot:({rot1}, {rot2}, {rot3}) | FOV: {fov}")

kernel32.CloseHandle(handle)
