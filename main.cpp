#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include "global.hpp"
#include "UI/Widget.hpp"

using namespace std;

std::vector<CHyprspaceWidget*> g_overviewWidgets;

CFunctionHook* renderWorkspaceWindowsHook;
CFunctionHook* arrangeLayersForMonitorHook;
CFunctionHook* recalculateMonitorHook;
CFunctionHook* changeWorkspaceHook;
CFunctionHook* getWorkspaceRulesForHook;

void* pRenderWindow;
void* pRenderLayer;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

CHyprspaceWidget* getWidgetForMonitor(CMonitor* pMonitor) {
    for (auto widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            return widget;
        }
    }
    return nullptr;
}

void hkRenderWorkspaceWindows(CHyprRenderer* thisptr, CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now) {
    auto widget = getWidgetForMonitor(pMonitor);
    if (widget) widget->draw(now);
    (*(tRenderWorkspaceWindows)renderWorkspaceWindowsHook->m_pOriginal)(thisptr, pMonitor, pWorkspace, now);
}

void hkArrangeLayersForMonitor(CHyprRenderer* thisptr, const int& monitor) {
    (*(tArrangeLayersForMonitor)arrangeLayersForMonitorHook->m_pOriginal)(thisptr, monitor);
    for (auto widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        auto pMonitor = g_pCompositor->getMonitorFromID(monitor);
        if (widget->getOwner() == pMonitor) {
            if (widget->reserveArea() > pMonitor->vecReservedTopLeft.y) {
                int oActiveWorkspace = pMonitor->activeWorkspace;

                for (auto& ws : g_pCompositor->m_vWorkspaces) { // HACK: recalculate other workspaces without reserved area
                    if (ws->m_iMonitorID == monitor && ws->m_iID != oActiveWorkspace) {
                        pMonitor->activeWorkspace = ws->m_iID;
                        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitor);
                    }
                }
                pMonitor->activeWorkspace = oActiveWorkspace;
                pMonitor->vecReservedTopLeft.y = widget->reserveArea();
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitor);
            }
            break;
        }
    }
}

// shit crashes on hook and idk why, workaround in arrangelayersformonitor
void hkRecalculateMonitor(void* thisptr, const int& monid) {
    auto pMonitor = g_pCompositor->getMonitorFromID(monid);

    if (!pMonitor) return;

    float oReservedTop = pMonitor->vecReservedTopLeft.y;
    auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromID(monid));
    if (widget) pMonitor->vecReservedTopLeft.y = widget->reserveArea();
    (*(tRecalculateMonitor)recalculateMonitorHook->m_pOriginal)(thisptr, monid);
    pMonitor->vecReservedTopLeft.y = oReservedTop;

}


void hkChangeWorkspace(CMonitor* thisptr, CWorkspace* pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    (*(tChangeWorkspace)changeWorkspaceHook->m_pOriginal)(thisptr, pWorkspace, internal, noMouseMove, noFocus);
    auto widget = getWidgetForMonitor(thisptr);
    if (widget)
        if (widget->isActive())
            widget->show();
}

std::vector<SWorkspaceRule> hkGetWorkspaceRulesFor(CConfigManager* thisptr, CWorkspace* pWorkspace) {
    auto oReturn = (*(tGetWorkspaceRulesFor)getWorkspaceRulesForHook->m_pOriginal)(thisptr, pWorkspace);
    auto pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    if (pMonitor->activeWorkspace == pWorkspace->m_iID) {
        auto widget = getWidgetForMonitor(pMonitor);
        if (widget)
            if (widget->isActive()) {
                auto rule = SWorkspaceRule();
                rule.gapsIn = 50;
                rule.gapsOut = 50;
                oReturn.push_back(rule);
            }
    }

    return oReturn;
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget)
        widget->isActive() ? widget->hide() : widget->show();
}


APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    if (!HyprlandAPI::addDispatcher(pHandle, "overview:toggle", dispatchToggleOverview)) return {};

    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWorkspaceWindows");
    renderWorkspaceWindowsHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkRenderWorkspaceWindows);
    if (renderWorkspaceWindowsHook)
        renderWorkspaceWindowsHook->hook();

    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "arrangeLayersForMonitor");
    arrangeLayersForMonitorHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkArrangeLayersForMonitor);
    if (arrangeLayersForMonitorHook)
        arrangeLayersForMonitorHook->hook();

    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "changeWorkspace");
    changeWorkspaceHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkChangeWorkspace);
    if (changeWorkspaceHook)
        changeWorkspaceHook->hook();

    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "getWorkspaceRulesFor");
    getWorkspaceRulesForHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkGetWorkspaceRulesFor);
    if (getWorkspaceRulesForHook)
        getWorkspaceRulesForHook->hook();

    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWindow");
    pRenderWindow = funcSearch[0].address;

    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderLayer");
    pRenderLayer = funcSearch[0].address;

    //recalculateMonitorHook = HyprlandAPI::createFunctionHook(pHandle, (void*)&CHyprDwindleLayout::recalculateMonitor, (void*)&hkRecalculateMonitor);
    //if (recalculateMonitorHook)
    //    recalculateMonitorHook->hook();
    //else return {};

    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_vMonitors) {
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.push_back(widget);
    }

    return {"Hyprspace", "Workspace switcher and overview plugin", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    renderWorkspaceWindowsHook->unhook();
    arrangeLayersForMonitorHook->unhook();
    changeWorkspaceHook->unhook();
    recalculateMonitorHook->unhook();
    getWorkspaceRulesForHook->unhook();
}