#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>

using namespace std;

inline HANDLE pHandle = NULL;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

#if defined(__x86_64__)

#elif defined(__aarch64__)

#else
#error "Unsupported architecture."
#endif

    return {"Hyprspace", "Workspace switcher and overview plugin", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {

}