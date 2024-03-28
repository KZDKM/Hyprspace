
// CFunctionHook reimplementation for ARM64
// not going anywhere until hyprland implements func hook for ARM

#pragma once

#include <string>
#include <vector>
#include <memory>

#define HANDLE void*

class CFunctionHookARM64 {
  public:
    CFunctionHookARM64(HANDLE owner, void* source, void* destination);
    ~CFunctionHookARM64();

    bool hook();
    bool unhook();

    CFunctionHookARM64(const CFunctionHookARM64&)            = delete;
    CFunctionHookARM64(CFunctionHookARM64&&)                 = delete;
    CFunctionHookARM64& operator=(const CFunctionHookARM64&) = delete;
    CFunctionHookARM64& operator=(CFunctionHookARM64&&)      = delete;

    void*          m_pOriginal = nullptr;

  private:
    void*  m_pSource         = nullptr;
    void*  m_pFunctionAddr   = nullptr;
    void*  m_pTrampolineAddr = nullptr;
    void*  m_pDestination    = nullptr;
    size_t m_iHookLen        = 0;
    size_t m_iTrampoLen      = 0;
    HANDLE m_pOwner          = nullptr;
    bool   m_bActive         = false;

    void*  m_pOriginalBytes = nullptr;

    struct SInstructionProbe {
        size_t              len      = 0;
        std::string         assembly = "";
        std::vector<size_t> insSizes;
    };

    struct SAssembly {
        std::vector<char> bytes;
    };

    SInstructionProbe probeMinimumJumpSize(void* start, size_t min);
    SInstructionProbe getInstructionLenAt(void* start);

    SAssembly         fixInstructionProbeRIPCalls(const SInstructionProbe& probe);

    friend class CHookSystem;
};

class CHookSystemARM64 {
  public:
    CFunctionHookARM64* initHook(HANDLE handle, void* source, void* destination);
    bool           removeHook(CFunctionHookARM64* hook);

    void           removeAllHooksFrom(HANDLE handle);

  private:
    std::vector<std::unique_ptr<CFunctionHookARM64>> m_vHooks;
};

inline std::unique_ptr<CHookSystemARM64> g_pFunctionHookSystemARM64;