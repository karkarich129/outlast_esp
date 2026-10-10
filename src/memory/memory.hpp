#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <iostream>

class Memory {
public:
    Memory();
    ~Memory();

    bool Attach(const std::wstring& processName);
    void Detach();
    bool IsValid() const;

    DWORD GetProcessId() const { return m_processId; }
    uintptr_t GetModuleBase() const { return m_moduleBase; }
    size_t GetModuleSize() const { return m_moduleSize; }

    template<typename T>
    T Read(uintptr_t address) const {
        T buffer{};
        if (!m_hProcess || address == 0) return buffer;
        ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr);
        return buffer;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const {
        if (!m_hProcess || address == 0 || !buffer) return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
    }

    std::string ReadString(uintptr_t address, size_t maxLength = 64) const;
    std::wstring ReadWString(uintptr_t address, size_t maxLength = 64) const;

    // Pattern / signature scanner across module memory
    uintptr_t PatternScan(const char* pattern, const char* mask) const;
    uintptr_t FindSignature(const std::string& patternWithSpaces) const;

    // Follow multi-level pointer chain
    uintptr_t ReadPointerChain(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets) const;

private:
    HANDLE m_hProcess = nullptr;
    DWORD m_processId = 0;
    uintptr_t m_moduleBase = 0;
    size_t m_moduleSize = 0;

    DWORD FindProcessId(const std::wstring& processName);
    uintptr_t GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName, size_t& moduleSize);
};

inline Memory g_Memory;
