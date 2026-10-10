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
print(f"TRUE GNames Table Data: 0x{gnames_data:X}, Num: {gnames_num}")

def get_name(idx):
    if not gnames_data or idx < 0 or idx >= gnames_num: return ""
    entry = r64(gnames_data + idx * 8)
    if not entry: return ""
    # In 0x1400C97DF: test byte ptr [entry + 8], 1 -> 1 = Unicode, 0 = Ansi!
    # String begins at entry + 0x14!
    flags = r32(entry + 8)
    if flags & 1:
        return rstr_wide(entry + 0x14)
    else:
        return rstr_ansi(entry + 0x14)

print("First 20 GNames strings:")
for i in range(20):
    print(f"  [{i}]: '{get_name(i)}'")

# Test with our player controller:
pc = 0x7FF4D356A080
pawn = r64(pc + 0x248) # 0x7FF4CE3F0040
pawn_name = get_name(r32(pawn + 0x48))
pawn_class = get_name(r32(r64(pawn + 0x50) + 0x48))
print(f"\nCamera Actor: '{pawn_name}' (Class: '{pawn_class}')")

kernel32.CloseHandle(handle)
