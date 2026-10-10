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

def get_obj_name(obj):
    if not obj: return "NULL"
    name_idx = r32(obj + 0x50)
    uclass = r64(obj + 0x58)
    class_name = get_name(r32(uclass + 0x50)) if uclass else "?"
    return f"{get_name(name_idx)} ({class_name})"

uengine = r64(base + 0x2006F00)
uworld = r64(base + 0x200B5A8)

# Check GamePlayers in UEngine:
# In 64-bit TArray<ULocalPlayer*>:
# Is it at +0x6E0 or +0x6E4?
# Let's inspect memory at uengine + 0x6DC .. 0x6FC
print("UEngine + 0x6DC bytes:")
for o in range(0x6DC, 0x6FC, 4):
    print(f"  +0x{o:X}: 0x{r32(uengine + o):08X} (as u64: 0x{r64(uengine + o):X})")

# Let's test player_data pointer:
for test_off in [0x6E0, 0x6E4, 0x6E8]:
    p = r64(uengine + test_off)
    lp = r64(p) if p else 0
    print(f"\nTesting UEngine + 0x{test_off:X} -> p: 0x{p:X} -> [0]: 0x{lp:X} -> Object: {get_obj_name(lp)}")
    if lp and "Player" in get_obj_name(lp):
        print(f"  FOUND LOCAL PLAYER at 0x{lp:X}!")
        # Let's inspect LocalPlayer offsets:
        for lpoff in range(0x60, 0x90, 8):
            pc = r64(lp + lpoff)
            print(f"    LP + 0x{lpoff:X}: 0x{pc:X} -> {get_obj_name(pc)}")
            if pc and ("PlayerController" in get_obj_name(pc) or "Controller" in get_obj_name(pc)):
                print(f"      FOUND PLAYER CONTROLLER at 0x{pc:X}!")
                # Inspect PlayerController offsets:
                for pcoff in [0x350, 0x430, 0x434, 0x438, 0x440, 0x470, 0x478, 0x49C]:
                    val = r64(pc + pcoff)
                    fval = rf(pc + pcoff)
                    print(f"        PC + 0x{pcoff:X}: 0x{val:X} (float: {fval}) -> {get_obj_name(val)}")

kernel32.CloseHandle(handle)
