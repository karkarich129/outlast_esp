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

uworld = r64(base + 0x200B5A8)
print(f"UWorld: 0x{uworld:X}")

# Scan UWorld levels:
levels_arr = r64(uworld + 0x70)
levels_num = r32(uworld + 0x78)
print(f"Levels Array: 0x{levels_arr:X}, Num: {levels_num}")

all_actors = []
for l_idx in range(levels_num):
    lvl = r64(levels_arr + l_idx * 8)
    if not lvl: continue
    actors_arr = r64(lvl + 0x60)
    actors_num = r32(lvl + 0x68)
    print(f"  Level [{l_idx}] (0x{lvl:X}): Actors Array 0x{actors_arr:X}, Num: {actors_num}")
    if actors_arr and 0 < actors_num < 10000:
        for a_idx in range(actors_num):
            act = r64(actors_arr + a_idx * 8)
            if act:
                # read Location and Rotation
                lx = rf(act + 0x80)
                ly = rf(act + 0x84)
                lz = rf(act + 0x88)
                all_actors.append((act, lx, ly, lz))

print(f"Total non-null Actors across all levels: {len(all_actors)}")
for act, lx, ly, lz in all_actors[:20]:
    print(f"   Actor 0x{act:X} -> Pos: ({lx:.1f}, {ly:.1f}, {lz:.1f})")

# Look at PlayerController at LP + 0x68:
lp = 0x7FF4D4BE3900
pc = r64(lp + 0x68)
print(f"\nPlayerController at LP + 0x68: 0x{pc:X}")
if pc:
    pawn = r64(pc + 0x350)
    print(f"  Pawn (+0x350): 0x{pawn:X}")
    if pawn:
        print(f"    Pawn Location: ({rf(pawn+0x80):.1f}, {rf(pawn+0x84):.1f}, {rf(pawn+0x88):.1f})")
    
    # Let's search PlayerController for Pawn pointer:
    for off in range(0x0, 0x600, 8):
        v = r64(pc + off)
        if any(v == a[0] for a in all_actors):
            print(f"  MATCH: PC + 0x{off:X} = Actor 0x{v:X}!")
        fv = rf(pc + off)
        if 40.0 <= fv <= 120.0 and (off % 4 == 0):
            print(f"  Potential FOV at PC + 0x{off:X}: {fv}")

kernel32.CloseHandle(handle)
