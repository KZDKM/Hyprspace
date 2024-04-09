#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include "Overview.hpp"
#include "Globals.hpp"
#include <dlfcn.h>

using namespace std;

CFunctionHook* renderWorkspaceWindowsHook;
CFunctionHook* arrangeLayersForMonitorHook;
CFunctionHook* recalculateMonitorHook;
CFunctionHook* changeWorkspaceHook;
CFunctionHook* getWorkspaceRuleForHook;
CFunctionHook* onMouseEventHook;
CFunctionHook* glTexParameteriHook;

void* pMouseKeybind;
void* pRenderWindow;
void* pRenderLayer;

std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;

bool g_useMipmapping = false;

APICALL EXPORT string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

std::shared_ptr<CHyprspaceWidget> getWidgetForMonitor(CMonitor* pMonitor) {
    for (auto& widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            return widget;
        }
    }
    return nullptr;
}

void hkRenderWorkspaceWindows(CHyprRenderer* thisptr, CMonitor* pMonitor, PHLWORKSPACE pWorkspace, timespec* now) {
    auto widget = getWidgetForMonitor(pMonitor);
    if (widget) widget->draw(now);
    float oAlpha = 1;
    if (widget->isActive() && g_pInputManager->currentlyDraggedWindow)
        if (g_pInputManager->currentlyDraggedWindow->m_iMonitorID == widget->getOwner()->ID) {
            oAlpha = g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.goal();
            g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.setValueAndWarp(0.2);
        }
    (*(tRenderWorkspaceWindows)renderWorkspaceWindowsHook->m_pOriginal)(thisptr, pMonitor, pWorkspace, now);
    if (widget->isActive() && g_pInputManager->currentlyDraggedWindow)
        if (g_pInputManager->currentlyDraggedWindow->m_iMonitorID == widget->getOwner()->ID) {
            g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.setValueAndWarp(oAlpha);
        }
}

// trigger recalculations for all workspace
void hkArrangeLayersForMonitor(CHyprRenderer* thisptr, const int& monitor) {
    (*(tArrangeLayersForMonitor)arrangeLayersForMonitorHook->m_pOriginal)(thisptr, monitor);
    auto pMonitor = g_pCompositor->getMonitorFromID(monitor);
    auto widget = getWidgetForMonitor(pMonitor);
    if (pMonitor && widget.get())
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
}


// currently this is only here to re-hide top layer panels on workspace change
//TODO: use event hook instead of function hook
void hkChangeWorkspace(CMonitor* thisptr, const PHLWORKSPACE& pWorkspace, bool internal, bool noMouseMove, bool noFocus) {
    (*(tChangeWorkspace)changeWorkspaceHook->m_pOriginal)(thisptr, pWorkspace, internal, noMouseMove, noFocus);
    auto widget = getWidgetForMonitor(thisptr);
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
        auto widget = getWidgetForMonitor(pMonitor);
        if (widget)
            if (widget->isActive()) {
                oReturn.gapsOut = 60;
                oReturn.gapsIn = 20;
            }
    }

    return oReturn;
}

// for generating mipmap on miniature windows

void hkGLTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (g_useMipmapping && pname == GL_TEXTURE_MIN_FILTER && param == GL_LINEAR) {
        param = GL_LINEAR_MIPMAP_LINEAR;
        glGenerateMipmap(target);
    }
    (*(tGLTexParameteri)glTexParameteriHook->m_pOriginal)(target, pname, param);
}

// for dragging windows into workspace
bool hkOnMouseEvent(CKeybindManager* thisptr, wlr_pointer_button_event* e) {

    bool oReturn;

    const auto pressed = e->state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    int targetWorkspaceID = -1;
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {

            // currentlyDraggedWindow would be cleared by the execution of original function
            const auto targetWindow = g_pInputManager->currentlyDraggedWindow;

            if (widget->isActive()) {
                // when overview is active, always drag windows on mouse click
                if (g_pInputManager->currentlyDraggedWindow) {
                    g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
                    g_pInputManager->currentlyDraggedWindow = nullptr;
                    g_pInputManager->dragMode = MBIND_INVALID;
                }
                std::string keybind = (pressed ? "1" : "0") + std::string("movewindow");
                (*(tMouseKeybind)pMouseKeybind)(keybind);
                oReturn = false;

                // find which workspace the mouse hovers over
                for (auto& w : widget->workspaceBoxes) {
                    auto wi = std::get<0>(w);
                    auto wb = std::get<1>(w);
                    if (wb.containsPoint(g_pInputManager->getMouseCoordsInternal())) {
                        targetWorkspaceID = wi;
                        break;
                    }
                }

                auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);

                // create new workspace
                if (!targetWorkspace && targetWorkspaceID >= 0) {
                    targetWorkspace = g_pCompositor->createNewWorkspace(targetWorkspaceID, widget->getOwner()->ID);
                }

                // release window on workspace to drop it in
                if (targetWindow && targetWorkspace.get() && !pressed) {
                    g_pCompositor->moveWindowToWorkspaceSafe(targetWindow, targetWorkspace);
                    if (targetWindow->m_bIsFloating) {
                        auto targetPos = pMonitor->vecPosition + (pMonitor->vecSize / 2.) - (targetWindow->m_vReportedSize / 2.);
                        targetWindow->m_vPosition = targetPos;
                        targetWindow->m_vRealPosition = targetPos;
                    }
                    g_pHyprRenderer->arrangeLayersForMonitor(widget->getOwner()->ID);
                }
                // click workspace to change to workspace and exit overview
                else if (targetWorkspace && pressed) {
                    g_pCompositor->getMonitorFromID(targetWorkspace->m_iMonitorID)->changeWorkspace(targetWorkspace->m_iID);
                    //if (widget->isActive()) widget->hide();
                }
                // click elsewhere to exit overview
                //else if (!targetWorkspace.get() && widget->isActive() && !pressed) widget->hide();
            }
            else
                oReturn = (*(tOnMouseEvent)onMouseEventHook->m_pOriginal)(thisptr, e);
        }
        else oReturn = (*(tOnMouseEvent)onMouseEventHook->m_pOriginal)(thisptr, e);
    }
    else oReturn = (*(tOnMouseEvent)onMouseEventHook->m_pOriginal)(thisptr, e);

    return oReturn;
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget)
        widget->isActive() ? widget->hide() : widget->show();
}

void dispatchCloseOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget)
        if (widget->isActive()) widget->hide();
}

void* findFunctionBySymbol(HANDLE inHandle, const std::string func, const std::string sym) {
    // should return all functions
    auto funcSearch = HyprlandAPI::findFunctionsByName(inHandle, func);
    for (auto f : funcSearch) {
        if (f.demangled.contains(sym))
            return f.address;
    }
    return nullptr;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    HyprlandAPI::addDispatcher(pHandle, "overview:toggle", dispatchToggleOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:close", dispatchCloseOverview);

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
    if (getWorkspaceRuleForHook)
        getWorkspaceRuleForHook->hook();

    // CKeybindManager::mouse (names too generic bruh)
    pMouseKeybind = findFunctionBySymbol(pHandle, "mouse", "CKeybindManager::mouse");
    onMouseEventHook = HyprlandAPI::createFunctionHook(pHandle, findFunctionBySymbol(pHandle, "onMouseEvent", "CKeybindManager::onMouseEvent"), (void*)&hkOnMouseEvent);
    if (onMouseEventHook)
        onMouseEventHook->hook();

    // gotta find other ways to hook
    //glTexParameteriHook = HyprlandAPI::createFunctionHook(pHandle, (void*)&glTexParameteri, (void*)hkGLTexParameteri);
    //if (glTexParameteriHook)
    //    glTexParameteriHook->hook();

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
    if (renderWorkspaceWindowsHook)
        renderWorkspaceWindowsHook->unhook();
    if (arrangeLayersForMonitorHook)
        arrangeLayersForMonitorHook->unhook();
    if (changeWorkspaceHook)
        changeWorkspaceHook->unhook();
    if (recalculateMonitorHook)
        recalculateMonitorHook->unhook();
    if (getWorkspaceRuleForHook)
        getWorkspaceRuleForHook->unhook();
    if (onMouseEventHook)
        onMouseEventHook->unhook();
    //if (glTexParameteriHook)
    //    glTexParameteriHook->unhook();
}