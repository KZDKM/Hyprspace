#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/Compositor.hpp>
#include "global.hpp"
#include "UI/Widget.hpp"

using namespace std;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

typedef void (*tRenderWorkspaceWindows)(void*, CMonitor*, CWorkspace*, timespec*);
tRenderWorkspaceWindows oRenderWorkspaceWindows;
CFunctionHook* renderWorkspaceWindowsHook;

std::vector<CHyprspaceWidget*> g_overviewWidgets;

void hkRenderWorkspaceWindows(void* thisptr, CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now) {
    for (auto widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            widget->draw(now);
            break;
        }
    }
    (*(tRenderWorkspaceWindows)renderWorkspaceWindowsHook->m_pOriginal)(thisptr, pMonitor, pWorkspace, now);
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();

    for (auto widget : g_overviewWidgets) {
        if (!widget) continue;
        if (widget->getOwner() == currentMonitor)
            widget->isActive() ? widget->hide() : widget->show();
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    if (!HyprlandAPI::addDispatcher(inHandle, "overview:toggle", dispatchToggleOverview)) return {};

    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWorkspaceWindows");
    if (!funcSearch[0].address) return {};
    renderWorkspaceWindowsHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkRenderWorkspaceWindows);
    if (renderWorkspaceWindowsHook)
        renderWorkspaceWindowsHook->hook();
    else return {};

    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_vMonitors) {
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.push_back(widget);
    }

    return {"Hyprspace", "Workspace switcher and overview plugin", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    renderWorkspaceWindowsHook->unhook();
}