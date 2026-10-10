import ctypes
from ctypes import wintypes
import subprocess

kernel32 = ctypes.windll.kernel32
psapi = ctypes.windll.psapi

cmd = 'powershell -Command "(Get-Process -Name OLGame).Id"'
res = subprocess.check_output(cmd, shell=True).decode().strip()
pid = int(res.split()[0])
handle = kernel32.OpenProcess(0x1F0FFF, False, pid)

hMods = (wintypes.HMODULE * 1024)()
cbNeeded = wintypes.DWORD()
psapi.EnumProcessModules(handle, ctypes.byref(hMods), ctypes.sizeof(hMods), ctypes.byref(cbNeeded))
base = hMods[0]

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

def rstr(addr, max_len=64):
    buf = ctypes.create_string_buffer(max_len)
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, max_len, ctypes.byref(read))
    return buf.value.decode('ascii', errors='ignore')

gnames = r64(base + 0x1FF8FE8)
def get_name(idx):
    if not gnames or idx < 0 or idx > 300000: return ""
    entry = r64(gnames + idx * 8)
    if not entry: return ""
    s = rstr(entry + 0x10)
    return s if s else rstr(entry + 0x18)

lp = 0x7FF4D4BE3900
print(f"Investigating object 0x{lp:X}:")
for off in range(0x0, 0x70, 8):
    val = r64(lp + off)
    val32 = r32(lp + off)
    print(f"  +0x{off:02X}: 0x{val:X} | as Name: '{get_name(val32)}'")

print("\nPointers inside 0x7FF4D4BE3900:")
for off in range(0x50, 0xA0, 8):
    p = r64(lp + off)
    print(f"  +0x{off:02X}: 0x{p:X}")
    if p > 0x10000:
        for inner in range(0x0, 0x40, 8):
            v32 = r32(p + inner)
            n = get_name(v32)
            if n:
                print(f"     Inner +0x{inner:X}: '{n}'")

kernel32.CloseHandle(handle)
