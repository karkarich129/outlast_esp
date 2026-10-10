import ctypes
from ctypes import wintypes
import subprocess, time

kernel32 = ctypes.windll.kernel32
cmd = 'powershell -Command "(Get-Process -Name OLGame).Id"'
res = subprocess.check_output(cmd, shell=True).decode().strip()
pid = int(res.split()[0])
handle = kernel32.OpenProcess(0x1F0FFF, False, pid)

def r32(addr):
    buf = ctypes.c_uint32()
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), ctypes.byref(buf), 4, ctypes.byref(read))
    return ctypes.c_int32(buf.value).value

def rf(addr):
    buf = ctypes.c_float()
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), ctypes.byref(buf), 4, ctypes.byref(read))
    return buf.value

pc = 0x7FF4D356A080
p_pitch = r32(pc + 0x8C)
p_yaw = r32(pc + 0x90)
p_roll = r32(pc + 0x94)

print(f"PlayerController Rotation at +0x8C: Pitch={p_pitch} ({p_pitch*360/65536:.1f} deg), Yaw={p_yaw} ({p_yaw*360/65536:.1f} deg)")
print(f"PlayerController Location at +0x80: X={rf(pc+0x80):.1f}, Y={rf(pc+0x84):.1f}, Z={rf(pc+0x88):.1f}")

kernel32.CloseHandle(handle)
