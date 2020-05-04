#include "pch.h"
#include <csignal>
#include <ImageHlp.h>
#include <Psapi.h>
#include "DebugUtils.h"
// #include "Version.h"

void DebugPrint(const std::string& text) {
#ifdef _DEBUG
    OutputDebugStringA(text.c_str());
    std::cout << text;
    if (text[text.size()-1] != '\n') {
        OutputDebugStringA("\n");
        std::cout << std::endl;
    }
#endif
}

std::wstring DebugUtils::GetStackTrace() {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymInitialize(process, NULL, TRUE); // For some reason, this is required in order to make StackWalk work.
    CONTEXT context;
    RtlCaptureContext(&context);
    STACKFRAME64 stackFrame;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    std::wstringstream ss;
    ss << std::hex << std::showbase << std::nouppercase;

    uint64_t baseAddress = GetBaseAddress();
    BOOL result = FALSE;
    do {
        ss << (stackFrame.AddrPC.Offset - baseAddress) << L' '; // Normalize offsets relative to the base address
        result = StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);
    } while (result == TRUE);
    return ss.str();
}

bool DebugUtils::s_isShowingError = false;
void DebugUtils::ShowAssertDialogue() {
    if (s_isShowingError) return;
    Memory::PauseHeartbeat();
    std::wstring msg = L"WitnessTrainer version " + version + L" has encountered an error.\n";
    msg += L"Click 'Yes' to ignore it and continue the program,\n";
    msg += L"or click 'No' to stop the program.\n\n";
    msg += L"In either case, please press Control C to copy this error,\n";
    msg += L"then paste it to darkid.\n";
    msg += GetStackTrace();
    s_isShowingError = true; // In case there's an assert firing *within* the WndProc, then we need to make sure not to keep firing, or we'll get into a loop.
    int action = MessageBox(NULL, msg.c_str(), L"WitnessTrainer encountered an error.",
        MB_TASKMODAL | MB_ICONHAND | MB_YESNO | MB_SETFOREGROUND);
    s_isShowingError = false;
    if (action == IDYES) { // User opts to ignore the exception, and continue execution
        Memory::ResumeHeartbeat();
        return;
    }
    // Else, we are aborting execution. Break into the debugger (if present), then exit.
    if (IsDebuggerPresent()) __debugbreak();
    raise(SIGABRT);
    _exit(3);
}

uint64_t DebugUtils::GetBaseAddress() {
    HANDLE process = GetCurrentProcess();
    DWORD unused;
    HMODULE modules[1];
    EnumProcessModules(process, &modules[0], sizeof(HMODULE), &unused);
    MODULEINFO moduleInfo;
    GetModuleInformation(process, modules[0], &moduleInfo, sizeof(moduleInfo));
    return reinterpret_cast<uint64_t>(moduleInfo.lpBaseOfDll);
}

// Note: This function must work properly even in release mode, since we will need to generate callbacks for release exes.
void DebugUtils::RegenerateCallstack(const std::wstring& callstack) {
    uint64_t baseAddress = GetBaseAddress();
    std::stringstream ss;
    ss << std::hex << std::showbase << std::nouppercase;
    std::wstring buffer;
    for (const wchar_t c : callstack) {
        if (c == L' ') {
            ss << baseAddress + std::stoull(buffer, nullptr, 16) << '\n';
            buffer.clear();
        } else {
            buffer += c;
            continue;
        }
    }
    ss << baseAddress + std::stoull(buffer, nullptr, 16) << '\n';
    OutputDebugStringA(ss.str().c_str());
}
