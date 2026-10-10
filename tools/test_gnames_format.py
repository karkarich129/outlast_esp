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

def rstr(addr, max_len=64):
    buf = ctypes.create_string_buffer(max_len)
    read = ctypes.c_size_t()
    kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, max_len, ctypes.byref(read))
    return buf.raw

# In OLGame.exe:
# Where is FNameTable / GNames?
# Earlier in offsets.hpp it was 0x1FF8FE8.
# In memory: base + 0x1FF8FE8 -> 0x7FF4FC270040.
# But what is at 0x7FF4FC270040?
# In UE3 64-bit, FNameTable is either:
# 1) TArray<FNameEntry*> GNames -> Data at +0x0, Num at +0x8
# 2) Chunked table: FNameEntry* Chunks[4096]
gnames_table = r64(base + 0x1FF8FE8)
print(f"Address at base + 0x1FF8FE8: 0x{gnames_table:X}")

for i in range(10):
    val = r64(gnames_table + i * 8)
    print(f"  +0x{i*8:02X}: 0x{val:X}")

# Let's search memory of OLGame.exe for string 'None' or 'ByteProperty'
for test_i in range(200):
    entry = r64(gnames_table + test_i * 8)
    if entry > 0x10000:
        raw = rstr(entry, 32)
        # check printable ascii:
        chars = [chr(b) for b in raw if 32 <= b <= 126]
        if len(chars) > 2:
            print(f"  Entry [{test_i}]: {''.join(chars)}")

kernel32.CloseHandle(handle)
