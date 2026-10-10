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
    return buf.value.decode('ascii', errors='ignore')

# In UE3 64-bit:
# struct UObject {
#    void* vftable;          // +0x00
#    int32 Index;            // +0x08
#    int32 HashNext;         // +0x0C
#    UObject* HashOuterNext; // +0x10
#    UState* StateFrame;     // +0x18
#    UObject* _Linker;       // +0x20
#    ...
#    UObject* Outer;         // +0x40
#    FName Name;             // +0x48 (Index: +0x48, Number: +0x4C)
#    UClass* Class;          // +0x50
#    ...
# }
# Notice: +0x48 was 0x1000025F1! Name Index = 0x25F1 (9713)!
# And +0x50 is UClass! (0x7FF4FD977380)

gnames = r64(base + 0x1FF8FE8)
print(f"GNames table ptr: 0x{gnames:X}")

def get_name(idx):
    if not gnames or idx < 0 or idx > 300000: return ""
    entry = r64(gnames + idx * 8)
    if not entry: return ""
    # In UE3 FNameEntry:
    # Flags/Index (8 bytes) + Union { Ansi[...], Wide[...] }
    for test_str_off in [0x10, 0x18, 0x08, 0x0C]:
        s = rstr(entry + test_str_off)
        if len(s) > 1 and s.isprintable():
            return s
    return ""

print("Test Name 0x25F1:", get_name(0x25F1))
uclass = 0x7FF4FD977380
class_name_idx = r32(uclass + 0x48)
print(f"UClass at 0x{uclass:X}, Class Name Index: 0x{class_name_idx:X} -> '{get_name(class_name_idx)}'")

# Let's inspect 0x7FF4D4BE3900 as ULocalPlayer:
# Where is PlayerController?
# In ULocalPlayer, let's find all pointers and their classes!
lp = 0x7FF4D4BE3900
for off in range(0x50, 0x120, 8):
    p = r64(lp + off)
    if p > 0x10000 and (p >> 32) in [0x7FF7, 0x7FF4]:
        c = r64(p + 0x50)
        c_name = get_name(r32(c + 0x48)) if c else ""
        obj_name = get_name(r32(p + 0x48))
        print(f"  LP + 0x{off:03X} -> 0x{p:X} : '{obj_name}' (Class: '{c_name}')")

kernel32.CloseHandle(handle)
