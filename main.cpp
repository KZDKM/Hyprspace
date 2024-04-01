#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/Compositor.hpp>
#include "UI/Widget.hpp"

using namespace std;

inline HANDLE pHandle = NULL;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

typedef void (*tRenderWorkspace)(void*, CMonitor*, CWorkspace*, timespec*, const CBox&);
tRenderWorkspace oRenderWorkspace;
CFunctionHook* renderWorkspaceHook;

std::vector<CHyprspaceWidget*> g_overviewWidgets;

void hkRenderWorkspace(void* thisptr, CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now, const CBox& geometry) {
    (*(tRenderWorkspace)renderWorkspaceHook->m_pOriginal)(thisptr, pMonitor, pWorkspace, now, geometry);
    g_pHyprOpenGL->m_RenderData.pWorkspace = pWorkspace;
    for (auto widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            widget->draw();
            break;
        }
    }
    g_pHyprOpenGL->m_RenderData.pWorkspace = nullptr;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWorkspace");
    renderWorkspaceHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkRenderWorkspace);
    renderWorkspaceHook->hook();

    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_vMonitors) {
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.push_back(widget);
    }

    return {"Hyprspace", "Workspace switcher and overview plugin", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    renderWorkspaceHook->unhook();
}