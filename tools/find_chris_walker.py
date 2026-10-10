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

def rstr_ansi(addr, max_len=64):
    buf = ctypes.create_string_buffer(max_len)
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, max_len, ctypes.byref(read))
    return buf.value.decode('ascii', errors='ignore')

def rstr_wide(addr, max_len=64):
    buf = ctypes.create_string_buffer(max_len * 2)
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, max_len * 2, ctypes.byref(read))
    return buf.raw.decode('utf-16le', errors='ignore').split('\x00')[0]

gnames_data = r64(base + 0x1FF8FA0)
gnames_num  = r32(base + 0x1FF8FA8)

def get_name(idx):
    if not gnames_data or idx < 0 or idx >= gnames_num: return ""
    entry = r64(gnames_data + idx * 8)
    if not entry: return ""
    flags = r32(entry + 8)
    if flags & 1:
        return rstr_wide(entry + 0x14)
    else:
        return rstr_ansi(entry + 0x14)

def get_class(act):
    if not act: return ""
    c = r64(act + 0x50)
    if not c: return ""
    return get_name(r32(c + 0x48))

# Scan GNames for anything containing walker, cannibal, chris, etc.
print("Scanning GNames for Chris Walker / Cannibal / Enemy names...")
for i in range(gnames_num):
    n = get_name(i)
    if any(k in n.lower() for k in ["cannibal", "walker", "chris", "surgeon", "trager", "nanocloud", "soldier", "enemy", "pawn"]):
        if not any(k in n.lower() for k in ["debris", "particle", "sound", "decal", "mat_", "tex_", "phys"]):
            print(f"  GNames [{i}]: '{n}'")

# Also scan all loaded actors in memory for cannibal / walker:
uworld = r64(base + 0x200B5A8)
levels_arr = r64(uworld + 0x70)
levels_num = r32(uworld + 0x78)

print(f"\nScanning loaded levels ({levels_num} levels)...")
for l_idx in range(levels_num):
    lvl = r64(levels_arr + l_idx * 8)
    if not lvl: continue
    actors_arr = r64(lvl + 0x60)
    actors_num = r32(lvl + 0x68)
    for a_idx in range(actors_num):
        act = r64(actors_arr + a_idx * 8)
        if not act: continue
        cname = get_class(act)
        name = get_name(r32(act + 0x48))
        if any(k in cname.lower() or k in name.lower() for k in ["cannibal", "walker", "chris", "enemy", "pawn", "npc", "soldier"]):
            pos = (rf(act+0x80), rf(act+0x84), rf(act+0x88))
            print(f"  Level [{l_idx}] Actor 0x{act:X} | Class: '{cname}' | Name: '{name}' | Pos: ({pos[0]:.1f}, {pos[1]:.1f}, {pos[2]:.1f})")

kernel32.CloseHandle(handle)
