#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

namespace Memory {

class ProcessMemory {
public:
    ProcessMemory() = default;
    ~ProcessMemory();

    ProcessMemory(const ProcessMemory&) = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;

    bool attach(const wchar_t* processName);
    void detach();
    bool attached() const { return m_process != nullptr; }
    bool alive() const;

    uint32_t pid() const { return m_pid; }
    uint32_t moduleBase() const { return m_moduleBase; }
    HANDLE handle() const { return m_process; }

    template <typename T>
    bool read(uint32_t address, T& value) const {
        SIZE_T got = 0;
        return m_process != nullptr && address != 0 &&
               ReadProcessMemory(
                   m_process,
                   reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),
                   &value,
                   sizeof(T),
                   &got) &&
               got == sizeof(T);
    }

    template <typename T>
    T read(uint32_t address) const {
        T value{};
        read(address, value);
        return value;
    }

    bool readRaw(uint32_t address, void* buffer, size_t size) const {
        SIZE_T got = 0;
        return m_process != nullptr && address != 0 &&
               ReadProcessMemory(
                   m_process,
                   reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),
                   buffer,
                   size,
                   &got) &&
               got == size;
    }

private:
    static void enableDebugPrivilege();
    static bool findProcess(const wchar_t* processName, uint32_t& pid);
    static bool findModuleBase(uint32_t pid, const wchar_t* moduleName, uint32_t& base);

    HANDLE m_process = nullptr;
    uint32_t m_pid = 0;
    uint32_t m_moduleBase = 0;
};

} // namespace Memory

