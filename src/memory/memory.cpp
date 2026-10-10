#include "memory.hpp"
#include <sstream>
#include <iomanip>

Memory::Memory() = default;

Memory::~Memory() {
    Detach();
}

bool Memory::Attach(const std::wstring& processName) {
    Detach();

    m_processId = FindProcessId(processName);
    if (m_processId == 0) {
        return false;
    }

    m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, m_processId);
    if (!m_hProcess) {
        return false;
    }

    m_moduleBase = GetModuleBaseAddress(m_processId, processName, m_moduleSize);
    return m_moduleBase != 0;
}

void Memory::Detach() {
    if (m_hProcess) {
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_processId = 0;
    m_moduleBase = 0;
    m_moduleSize = 0;
}

bool Memory::IsValid() const {
    if (!m_hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
        return true;
    }
    return false;
}

DWORD Memory::FindProcessId(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

uintptr_t Memory::GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName, size_t& moduleSize) {
    uintptr_t baseAddress = 0;
    moduleSize = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0) {
                baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                moduleSize = static_cast<size_t>(entry.modBaseSize);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return baseAddress;
}

std::string Memory::ReadString(uintptr_t address, size_t maxLength) const {
    if (!m_hProcess || address == 0 || maxLength == 0) return {};
    std::vector<char> buffer(maxLength + 1, 0);
    SIZE_T bytesRead = 0;
    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(address), buffer.data(), maxLength, &bytesRead);
    buffer[maxLength] = '\0';
    return std::string(buffer.data());
}

std::wstring Memory::ReadWString(uintptr_t address, size_t maxLength) const {
    if (!m_hProcess || address == 0 || maxLength == 0) return {};
    std::vector<wchar_t> buffer(maxLength + 1, 0);
    SIZE_T bytesRead = 0;
    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(address), buffer.data(), maxLength * sizeof(wchar_t), &bytesRead);
    buffer[maxLength] = L'\0';
    return std::wstring(buffer.data());
}

uintptr_t Memory::PatternScan(const char* pattern, const char* mask) const {
    if (!m_hProcess || m_moduleBase == 0 || m_moduleSize == 0) return 0;

    std::vector<uint8_t> dump(m_moduleSize);
    if (!ReadRaw(m_moduleBase, dump.data(), m_moduleSize)) {
        return 0;
    }

    size_t patternLen = strlen(mask);
    for (size_t i = 0; i < m_moduleSize - patternLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] != '?' && static_cast<uint8_t>(pattern[j]) != dump[i + j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return m_moduleBase + i;
        }
    }

    return 0;
}

uintptr_t Memory::FindSignature(const std::string& patternWithSpaces) const {
    std::vector<char> pattern;
    std::vector<char> mask;

    std::istringstream stream(patternWithSpaces);
    std::string byteStr;

    while (stream >> byteStr) {
        if (byteStr == "?" || byteStr == "??") {
            pattern.push_back(0);
            mask.push_back('?');
        } else {
            char byte = static_cast<char>(std::stoul(byteStr, nullptr, 16));
            pattern.push_back(byte);
            mask.push_back('x');
        }
    }

    mask.push_back('\0');
    return PatternScan(pattern.data(), mask.data());
}

uintptr_t Memory::ReadPointerChain(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets) const {
    uintptr_t current = baseAddress;
    for (size_t i = 0; i < offsets.size(); ++i) {
        current = Read<uintptr_t>(current + offsets[i]);
        if (current == 0) return 0;
    }
    return current;
}
