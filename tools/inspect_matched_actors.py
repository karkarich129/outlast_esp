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
print(f"Investigating PlayerController 0x{pc:X}:")

# Let's inspect matched actors:
# 1. 0x248, 0x250, 0x368, 0x4C0
for off, name in [(0x248, "0x248"), (0x250, "0x250"), (0x368, "0x368"), (0x4C0, "0x4C0"), (0x434, "0x434"), (0x430, "0x430"), (0x440, "0x440"), (0x478, "0x478")]:
    act = r64(pc + off)
    if act:
        lx = rf(act + 0x80)
        ly = rf(act + 0x84)
        lz = rf(act + 0x88)
        pitch = r32(act + 0x8C)
        yaw = r32(act + 0x90)
        roll = r32(act + 0x94)
        print(f"  PC + {name} -> 0x{act:X} | Pos: ({lx:.1f}, {ly:.1f}, {lz:.1f}) | Rot: ({pitch}, {yaw}, {roll})")
        # Check if this actor has camera data or is PlayerCamera:
        for inner in range(0x200, 0x300, 4):
            valf = rf(act + inner)
            if 40.0 <= valf <= 120.0:
                print(f"     Inner FOV candidate at +0x{inner:X}: {valf}")

# Also check camera location directly in PlayerCamera (which actor is PlayerCamera?)
for off in range(0x400, 0x500, 8):
    p = r64(pc + off)
    if p > 0x10000:
        # Check if p has float FOV or vector
        f = rf(p + 0x250)
        f2 = rf(p + 0x270)
        lx = rf(p + 0x270)
        if 40.0 <= f <= 120.0 or 40.0 <= f2 <= 120.0 or abs(lx) > 10.0:
            print(f"  PC + 0x{off:X} -> 0x{p:X} (f250: {f}, f270: {f2})")

kernel32.CloseHandle(handle)
