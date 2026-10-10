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

pc = 0x7FF4D356A080
print(f"Investigating PlayerController 0x{pc:X} (0x0 to 0x500):")
print(f"PC Rotation (+0x8C): ({r32(pc+0x8C)}, {r32(pc+0x90)}, {r32(pc+0x94)})")

for off in range(0x0, 0x600, 4):
    v = r32(pc + off)
    f = rf(pc + off)
    # Check if 3 consecutive values look like FRotator (pitch in [-16384, 16384], yaw in [0, 65535])
    p = r32(pc + off)
    y = r32(pc + off + 4)
    r = r32(pc + off + 8)
    # Pitch is signed int32, usually between -16384 and 16384 (or -32768 to 32768)
    sp = ctypes.c_int32(p).value
    sy = ctypes.c_int32(y).value
    sr = ctypes.c_int32(r).value
    if abs(sp) <= 16384 and 0 <= sy <= 65536 and abs(sr) <= 16384 and (sp != 0 or sy != 0):
        print(f"  Rotator Candidate at PC + 0x{off:X}: Pitch={sp}, Yaw={sy}, Roll={sr}")

kernel32.CloseHandle(handle)
