#include "ProcessMemory.h"

#include <tlhelp32.h>

namespace Memory {

ProcessMemory::~ProcessMemory() {
    detach();
}

void ProcessMemory::enableDebugPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return;
    }

    LUID luid{};
    if (LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) {
        TOKEN_PRIVILEGES privileges{};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
    }

    CloseHandle(token);
}

bool ProcessMemory::findProcess(const wchar_t* processName, uint32_t& pid) {
    pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{ sizeof(entry) };
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, processName) == 0) {
            pid = entry.th32ProcessID;
            break;
        }
    }

    CloseHandle(snapshot);
    return pid != 0;
}

bool ProcessMemory::findModuleBase(uint32_t pid, const wchar_t* moduleName, uint32_t& base) {
    base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W entry{ sizeof(entry) };
    for (BOOL ok = Module32FirstW(snapshot, &entry); ok; ok = Module32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szModule, moduleName) == 0) {
            base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry.modBaseAddr));
            break;
        }
    }

    CloseHandle(snapshot);
    return base != 0;
}

bool ProcessMemory::attach(const wchar_t* processName) {
    if (attached()) {
        if (alive()) {
            return true;
        }
        detach();
    }

    enableDebugPrivilege();

    uint32_t pid = 0;
    uint32_t base = 0;
    if (!findProcess(processName, pid) || !findModuleBase(pid, processName, base)) {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        return false;
    }

    m_process = process;
    m_pid = pid;
    m_moduleBase = base;
    return true;
}

void ProcessMemory::detach() {
    if (m_process) {
        CloseHandle(m_process);
    }
    m_process = nullptr;
    m_pid = 0;
    m_moduleBase = 0;
}

bool ProcessMemory::alive() const {
    if (!m_process) {
        return false;
    }

    DWORD code = 0;
    return GetExitCodeProcess(m_process, &code) && code == STILL_ACTIVE;
}

} // namespace Memory

