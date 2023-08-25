#pragma once
#include "ThreadSafeAddressMap.h"

enum ProcStatus : WPARAM {
    NotRunning,
    Started,
    Running,
    Reload,
    NewGame,
    Stopped
};

using byte = unsigned char;

// https://github.com/erayarslan/WriteProcessMemory-Example
// http://stackoverflow.com/q/32798185
// http://stackoverflow.com/q/36018838
// http://stackoverflow.com/q/1387064
// https://github.com/fkloiber/witness-trainer/blob/master/source/foreign_process_memory.cpp
class Memory final : public std::enable_shared_from_this<Memory> {
public:
    Memory(const std::wstring& processName);
    ~Memory();
    void StartHeartbeat(HWND window, UINT message);
    void StopHeartbeat();
    void BringToFront();
    bool IsForeground();

    static HWND GetProcessHwnd(DWORD pid);

    Memory(const Memory& memory) = delete;
    Memory& operator=(const Memory& other) = delete;

    // bytesToEOL is the number of bytes from the given index to the end of the opcode. Usually, the target address is last 4 bytes, since it's the destination of the call.
    static __int64 ReadStaticInt(__int64 offset, int index, const std::vector<byte>& data, size_t bytesToEOL = 4);
    using ScanFunc = std::function<void(__int64 offset, int index, const std::vector<byte>& data)>;
    using ScanFunc2 = std::function<bool(__int64 offset, int index, const std::vector<byte>& data)>;
    void AddSigScan(const std::vector<byte>& scanBytes, const ScanFunc& scanFunc);
    void AddSigScan2(const std::vector<byte>& scanBytes, const ScanFunc2& scanFunc);
    [[nodiscard]] size_t ExecuteSigScans();

    template<class T>
    inline std::vector<T> ReadData(const std::vector<__int64>& offsets, size_t numItems) {
        std::vector<T> data(numItems);
        if (!_handle) return data;
        ReadDataInternal(&data[0], ComputeOffset(offsets), numItems * sizeof(T));
        return data;
    }
    template<class T>
    inline std::vector<T> ReadAbsoluteData(const std::vector<__int64>& offsets, size_t numItems) {
        std::vector<T> data(numItems);
        if (!_handle) return data;
        ReadDataInternal(&data[0], ComputeOffset(offsets, true), numItems * sizeof(T));
        return data;
    }
    std::string ReadString(std::vector<__int64> offsets);

    template <class T>
    inline void WriteData(const std::vector<__int64>& offsets, const std::vector<T>& data) {
        WriteDataInternal(&data[0], ComputeOffset(offsets), sizeof(T) * data.size());
    }

    // This is the fully typed function -- you mostly won't need to call this.
    int CallFunction(__int64 address,
        const __int64 rcx, const __int64 rdx, const __int64 r8, const __int64 r9,
        const float xmm0, const float xmm1, const float xmm2, const float xmm3);
    int CallFunction(__int64 address, __int64 rcx) { return CallFunction(address, rcx, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f); }
    int CallFunction(__int64 address, __int64 rcx, __int64 rdx, __int64 r8, __int64 r9) { return CallFunction(address, rcx, rdx, r8, r9, 0.0f, 0.0f, 0.0f, 0.0f); }
    int CallFunction(__int64 address, __int64 rcx, const float xmm1) { return CallFunction(address, rcx, 0, 0, 0, 0.0f, xmm1, 0.0f, 0.0f); }
    int CallFunction(__int64 address, const std::string& str);

private:
    void Heartbeat(HWND window, UINT message);
    void Initialize();

    void ReadDataInternal(void* buffer, const uintptr_t computedOffset, size_t bufferSize);
    void WriteDataInternal(const void* buffer, uintptr_t computedOffset, size_t bufferSize);
    uintptr_t ComputeOffset(std::vector<__int64> offsets, bool absolute = false);
    uintptr_t AllocateArray(__int64 size);

    // Parts of the constructor / StartHeartbeat
    std::wstring _processName;
    bool _threadActive = false;
    std::thread _thread;

    // Parts of Initialize / heartbeat
    HANDLE _handle = nullptr;
    DWORD _pid = 0;
    HWND _hwnd = NULL;
    uintptr_t _baseAddress = 0;
    uintptr_t _endOfModule = 0;
    __int64 _globals = 0;
    int _loadCountOffset = 0;
    __int64 _previousEntityManager = 0;
    int _previousLoadCount = 0;
    ProcStatus _nextStatus = ProcStatus::Started;
    bool _trainerHasStarted = false;

#ifdef NDEBUG
    static constexpr std::chrono::milliseconds s_heartbeat = std::chrono::milliseconds(100);
#else // Induce more stress in debug, to catch errors more easily.
    static constexpr std::chrono::milliseconds s_heartbeat = std::chrono::milliseconds(10);
#endif


    // Parts of Read / Write / Sigscan / etc
    uintptr_t _functionPrimitive = 0;
    ThreadSafeAddressMap _computedAddresses;

    struct SigScan {
        bool found = false;
        ScanFunc2 scanFunc;
    };
    std::map<std::vector<byte>, SigScan> _sigScans;
};
