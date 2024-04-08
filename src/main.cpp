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
CFunctionHook* getWorkspaceRuleForHook;
CFunctionHook* onMouseEventHook;

void* pMouseKeybind;
void* pRenderWindow;
void* pRenderLayer;

// FIXME: this should be a static member of the class
std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkRenderWorkspaceWindows(CHyprRenderer* thisptr, CMonitor* pMonitor, PHLWORKSPACE pWorkspace, timespec* now) {
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
                const auto oActiveWorkspace = pMonitor->activeWorkspace;

                for (auto& ws : g_pCompositor->m_vWorkspaces) { // HACK: recalculate other workspaces without reserved area
                    if (ws->m_iMonitorID == monitor && ws->m_iID != oActiveWorkspace->m_iID) {
                        pMonitor->activeWorkspace = ws;
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


// currently this is only here to re-hide top layer panels on workspace change
//TODO: use event hook instead of function hook
void hkChangeWorkspace(CMonitor* thisptr, const PHLWORKSPACE& pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    (*(tChangeWorkspace)changeWorkspaceHook->m_pOriginal)(thisptr, pWorkspace, internal, noMouseMove, noFocus);
    auto widget = CHyprspaceWidget::getWidgetForMonitor(thisptr);
    if (widget.get())
        if (widget->isActive())
            widget->show();
}

// override gaps
//FIXME: gaps kept getting applied to every workspace
SWorkspaceRule hkGetWorkspaceRuleFor(CConfigManager* thisptr, PHLWORKSPACE pWorkspace) {
    auto oReturn = (*(tGetWorkspaceRuleFor)getWorkspaceRuleForHook->m_pOriginal)(thisptr, pWorkspace);
    auto pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    if (pMonitor->activeWorkspace == pWorkspace) {
        auto widget = CHyprspaceWidget::getWidgetForMonitor(pMonitor);
        if (widget)
            if (widget->isActive()) {
                oReturn.gapsIn = 50;
                oReturn.gapsOut = 50;
            }
    }

    return oReturn;
}

// for dragging windows into workspace
bool hkOnMouseEvent(CKeybindManager* thisptr, wlr_pointer_button_event* e) {

    auto targetWindow = g_pInputManager->currentlyDraggedWindow;

    bool oReturn = false;


    // when widget is active, always drag windows on click
    const auto pressed = e->state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    int targetWorkspaceID = -1;
    bool shouldDrag = false;
    if (pMonitor) {
        const auto widget = CHyprspaceWidget::getWidgetForMonitor(pMonitor);
        if (widget) {
            for (auto& w : widget->workspaceBoxes) {
                auto wi = std::get<0>(w);
                auto wb = std::get<1>(w);
                if (wb.containsPoint(g_pInputManager->getMouseCoordsInternal())) {
                    // null permissive
                    targetWorkspaceID = wi;
                    break;
                }
            }

            if (widget->isActive()) {
                shouldDrag = true;
            }
            // FIXME: autodrag still sends mouse event
            if (shouldDrag && pressed) {
                if (g_pInputManager->currentlyDraggedWindow) {
                    g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
                    g_pInputManager->currentlyDraggedWindow = nullptr;
                    g_pInputManager->dragMode = MBIND_INVALID;
                }
                std::string keybind = (pressed ? "1" : "0") + std::string("movewindow");
                (*(tMouseKeybind)pMouseKeybind)(keybind);
                oReturn = false;
            } else
                oReturn = (*(tOnMouseEvent)onMouseEventHook->m_pOriginal)(thisptr, e);

            auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);
            if (targetWindow && targetWorkspace.get() && !pressed) {
                g_pCompositor->moveWindowToWorkspaceSafe(targetWindow, targetWorkspace);
                if (targetWindow->m_bIsFloating) {
                    auto targetPos = pMonitor->vecPosition + (pMonitor->vecSize / 2.) - (targetWindow->m_vReportedSize / 2.);
                    targetWindow->m_vPosition = targetPos;
                    targetWindow->m_vRealPosition = targetPos;
                }
            }
            else if (targetWorkspace && pressed) {
                g_pCompositor->getMonitorFromID(targetWorkspace->m_iMonitorID)->changeWorkspace(targetWorkspace->m_iID);
                if (widget->isActive()) widget->hide();
            }
            else if (!targetWorkspace.get() && widget->isActive() && !pressed) widget->hide();
        }
    }

    return oReturn;
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = CHyprspaceWidget::getWidgetForMonitor(currentMonitor);
    if (widget)
        widget->isActive() ? widget->hide() : widget->show();
}

void* findFunctionBySymbol(HANDLE inHandle, const std::string func, const std::string sym) {

    // should return all functions
    auto funcSearch = HyprlandAPI::findFunctionsByName(inHandle, func);

    for (auto f : funcSearch) {
        if (f.demangled.contains(sym)) {
            return f.address;
        }
    }

    return 0;
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
    changeWorkspaceHook = HyprlandAPI::createFunctionHook(pHandle, findFunctionBySymbol(pHandle, "changeWorkspace", "CMonitor::changeWorkspace(std::shared_ptr<CWorkspace> const&, bool, bool, bool)"), (void*)&hkChangeWorkspace);
    if (changeWorkspaceHook)
        changeWorkspaceHook->hook();

    // CConfigManager::getWorkspaceRuleFor
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "getWorkspaceRuleFor");
    getWorkspaceRuleForHook = HyprlandAPI::createFunctionHook(pHandle, funcSearch[0].address, (void*)&hkGetWorkspaceRuleFor);
    //if (getWorkspaceRuleForHook)
    //    getWorkspaceRuleForHook->hook();

    // CKeybindManager::mouse (names too generic bruh)
    pMouseKeybind = findFunctionBySymbol(pHandle, "mouse", "CKeybindManager::mouse");
    onMouseEventHook = HyprlandAPI::createFunctionHook(pHandle, findFunctionBySymbol(pHandle, "onMouseEvent", "CKeybindManager::onMouseEvent"), (void*)&hkOnMouseEvent);
    if (onMouseEventHook)
        onMouseEventHook->hook();

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
    getWorkspaceRuleForHook->unhook();
    onMouseEventHook->unhook();
}