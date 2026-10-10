import ctypes
from ctypes import wintypes
import subprocess

kernel32 = ctypes.windll.kernel32
psapi = ctypes.windll.psapi

cmd = 'powershell -Command "(Get-Process -Name OLGame).Id"'
res = subprocess.check_output(cmd, shell=True).decode().strip()
pids = [int(p) for p in res.split() if p.isdigit()]
if not pids:
    print("OLGame not found!")
    exit(1)

pid = pids[0]
handle = kernel32.OpenProcess(0x1F0FFF, False, pid)
print(f"Attached to OLGame PID: {pid}")

hMods = (wintypes.HMODULE * 1024)()
cbNeeded = wintypes.DWORD()
psapi.EnumProcessModules(handle, ctypes.byref(hMods), ctypes.sizeof(hMods), ctypes.byref(cbNeeded))
base = hMods[0]
print(f"Base Address: 0x{base:X}")

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

gworld_ptr = base + 0x200B5A8
gengine_ptr = base + 0x2006F00

uworld = r64(gworld_ptr)
uengine = r64(gengine_ptr)

print(f"GWorld (0x{gworld_ptr:X}) -> 0x{uworld:X}")
print(f"GEngine (0x{gengine_ptr:X}) -> 0x{uengine:X}")

if uworld:
    print(f"\n--- UWorld Analysis (0x{uworld:X}) ---")
    for off in range(0x60, 0xA8, 8):
        val = r64(uworld + off)
        num = r32(uworld + off + 8)
        print(f"  UWorld + 0x{off:02X}: 0x{val:X} (Num: {num})")

if uengine:
    print(f"\n--- UEngine Analysis (0x{uengine:X}) ---")
    for off in [0x6E0, 0x6E4, 0x6E8, 0x6EC, 0x6F0, 0x80, 0x88, 0x90]:
        val = r64(uengine + off)
        num = r32(uengine + off + 8)
        print(f"  UEngine + 0x{off:03X}: 0x{val:X} (as 32: {r32(uengine + off)})")

kernel32.CloseHandle(handle)
