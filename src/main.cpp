#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include "Globals.hpp"
#include "Overview.hpp"

using namespace std;

CFunctionHook* renderWorkspaceWindowsHook;
CFunctionHook* arrangeLayersForMonitorHook;
CFunctionHook* recalculateMonitorHook;
CFunctionHook* changeWorkspaceHook;
CFunctionHook* getWorkspaceRulesForHook;
CFunctionHook* onMouseButtonHook;

void* pRenderWindow;
void* pRenderLayer;

std::vector<std::unique_ptr<CHyprspaceWidget>> g_overviewWidgets;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkRenderWorkspaceWindows(CHyprRenderer* thisptr, CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now) {
    auto widget = CHyprspaceWidget::getWidgetForMonitor(pMonitor);
    if (widget) widget->draw(now);
    (*(tRenderWorkspaceWindows)renderWorkspaceWindowsHook->m_pOriginal)(thisptr, pMonitor, pWorkspace, now);
}

// trigger recalculations for all workspace
void hkArrangeLayersForMonitor(CHyprRenderer* thisptr, const int& monitor) {
    (*(tArrangeLayersForMonitor)arrangeLayersForMonitorHook->m_pOriginal)(thisptr, monitor);
    for (auto& widget : g_overviewWidgets) {
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


// currently this is only here to re-hide panels on workspace change
//TODO: use event hook instead of function hook
void hkChangeWorkspace(CMonitor* thisptr, CWorkspace* pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    (*(tChangeWorkspace)changeWorkspaceHook->m_pOriginal)(thisptr, pWorkspace, internal, noMouseMove, noFocus);
    auto widget = CHyprspaceWidget::getWidgetForMonitor(thisptr);
    if (widget)
        if (widget->isActive())
            widget->show();
}

// override gaps
//FIXME: gaps kept getting applied to every workspace
std::vector<SWorkspaceRule> hkGetWorkspaceRulesFor(CConfigManager* thisptr, CWorkspace* pWorkspace) {
    auto oReturn = (*(tGetWorkspaceRulesFor)getWorkspaceRulesForHook->m_pOriginal)(thisptr, pWorkspace);
    auto pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    if (pMonitor->activeWorkspace == pWorkspace->m_iID) {
        auto widget = CHyprspaceWidget::getWidgetForMonitor(pMonitor);
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

// for dragging windows into workspace
void hkOnMouseButton(std::string args) {


    // when widget is active, always drag windows on click
    auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        auto widget = CHyprspaceWidget::getWidgetForMonitor(pMonitor);
        if (widget) {
            for (auto& w : widget->workspaceBoxes) {
                auto pWorkspace = std::get<0>(w);
                auto& wb = std::get<1>(w);
                if (wb->containsPoint(g_pInputManager->getMouseCoordsInternal())) { 
                    // TODO: implement window  and drop
                    break;
                }
            }

            if (widget->isActive())
                args = args[0] + "movewindow";
        }
    }
    (*(tOnMouseButton)onMouseButtonHook->m_pOriginal)(args);
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = CHyprspaceWidget::getWidgetForMonitor(currentMonitor);
    if (widget)
        widget->isActive() ? widget->hide() : widget->show();
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    if (!HyprlandAPI::addDispatcher(pHandle, "overview:toggle", dispatchToggleOverview)) return {};

    // CHyprRenderer::renderWorkspaceWindows
    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWorkspaceWindows");
    renderWorkspaceWindowsHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkRenderWorkspaceWindows);
    if (renderWorkspaceWindowsHook)
        renderWorkspaceWindowsHook->hook();

    // CHyprRenderer::arrangeLayersForMonitor
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "arrangeLayersForMonitor");
    arrangeLayersForMonitorHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkArrangeLayersForMonitor);
    if (arrangeLayersForMonitorHook)
        arrangeLayersForMonitorHook->hook();

    // CMonitor::changeWorkspace
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "changeWorkspace");
    changeWorkspaceHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkChangeWorkspace);
    if (changeWorkspaceHook)
        changeWorkspaceHook->hook();

    // CConfigManager::getWorkspaceRulesFor
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "getWorkspaceRulesFor");
    getWorkspaceRulesForHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkGetWorkspaceRulesFor);
    if (getWorkspaceRulesForHook)
        getWorkspaceRulesForHook->hook();

    // CKeybindManager::mouse (names too generic bruh)
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "mouse");
    onMouseButtonHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkOnMouseButton);

    // CHyprRenderer::renderWindow
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWindow");
    pRenderWindow = funcSearch[0].address;

    // CHyprRenderer::renderLayer
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderLayer");
    pRenderLayer = funcSearch[0].address;

    // create a widget for each monitor
    // TODO: update on monitor change
    for (auto& m : g_pCompositor->m_vMonitors) {
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.emplace_back(widget);
    }

    return {"Hyprspace", "Workspace overview", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    renderWorkspaceWindowsHook->unhook();
    arrangeLayersForMonitorHook->unhook();
    changeWorkspaceHook->unhook();
    recalculateMonitorHook->unhook();
    getWorkspaceRulesForHook->unhook();
}