#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include "Overview.hpp"
#include "Globals.hpp"

using namespace std;

CFunctionHook* renderWorkspaceWindowsHook;
CFunctionHook* arrangeLayersForMonitorHook;
CFunctionHook* recalculateMonitorHook;
CFunctionHook* changeWorkspaceHook;
CFunctionHook* getWorkspaceRuleForHook;
CFunctionHook* onMouseEventHook;
CFunctionHook* onAxisEventHook;
CFunctionHook* glTexParameteriHook;

void* pMouseKeybind;
void* pRenderWindow;
void* pRenderLayer;

std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;


CColor Config::panelBaseColor = CColor(0, 0, 0, 0);
CColor Config::workspaceActiveBackground = CColor(0, 0, 0, 0.25);
CColor Config::workspaceInactiveBackground = CColor(0, 0, 0, 0.5);
CColor Config::workspaceActiveBorder = CColor(1, 1, 1, 0.3);
CColor Config::workspaceInactiveBorder = CColor(1, 1, 1, 0);

int Config::panelHeight = 250;
int Config::workspaceMargin = 12;
int Config::workspaceBorderSize = 1;
bool Config::adaptiveHeight = false; // TODO: implement
bool Config::centerAligned = true;
bool Config::onTop = true; // TODO: implement
bool Config::hideBackgroundLayers = false;
bool Config::drawActiveWorkspace = true;

bool Config::overrideGaps = true;
int Config::gapsIn = 20;
int Config::gapsOut = 60;

bool Config::autoDrag = true;
bool Config::autoScroll = true;
bool Config::exitOnClick = true;
bool Config::switchOnDrop = false;
bool Config::exitOnSwitch = false;
bool Config::showNewWorkspace = true;
bool Config::showEmptyWorkspace = true;

float Config::dragAlpha = 0.2;

int hyprsplitNumWorkspaces = -1;

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
            g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.setValueAndWarp(Config::dragAlpha);
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
    if (pMonitor) {
        auto widget = getWidgetForMonitor(pMonitor);
        if (widget)
            widget->reserveArea();
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
    if (Config::overrideGaps) {
        auto pMonitor = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
        if (pMonitor->activeWorkspace == pWorkspace) {
            auto widget = getWidgetForMonitor(pMonitor);
            if (widget)
                if (widget->isActive()) {
                    oReturn.gapsOut = Config::gapsOut;
                    oReturn.gapsIn = Config::gapsIn;
                }
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

    const auto pressed = e->state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                return widget->mouseEvent(pressed);
            }
        }
    }

    return (*(tOnMouseEvent)onMouseEventHook->m_pOriginal)(thisptr, e);
}

// for scrolling through panel and switching workspace

bool hkOnAxisEvent(CKeybindManager* thisptr, wlr_pointer_axis_event* e) {
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                if (e->source == WL_POINTER_AXIS_SOURCE_WHEEL && e->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
                    return widget->axisEvent(e->delta);
            }
        }
    }

    return (*(tOnAxisEvent)onAxisEventHook->m_pOriginal)(thisptr, e);
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget)
        widget->isActive() ? widget->hide() : widget->show();
}

void dispatchOpenOverview(std::string arg) {
    if (arg.contains("all")) {
        for (auto& widget : g_overviewWidgets) {
            if (!widget->isActive()) widget->show();
        }
    }
    else {
        auto currentMonitor = g_pCompositor->getMonitorFromCursor();
        auto widget = getWidgetForMonitor(currentMonitor);
        if (widget)
            if (!widget->isActive()) widget->show();
    }
}

void dispatchCloseOverview(std::string arg) {
    if (arg.contains("all")) {
        for (auto& widget : g_overviewWidgets) {
            if (widget->isActive()) widget->hide();
        }
    }
    else {
        auto currentMonitor = g_pCompositor->getMonitorFromCursor();
        auto widget = getWidgetForMonitor(currentMonitor);
        if (widget)
            if (widget->isActive()) widget->hide();
    }
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

void reloadConfig() {
    Config::panelBaseColor = CColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelColor")->getValue()));
    Config::workspaceActiveBackground = CColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceActiveBackground")->getValue()));
    Config::workspaceInactiveBackground = CColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceInactiveBackground")->getValue()));
    Config::workspaceActiveBorder = CColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceActiveBorder")->getValue()));
    Config::workspaceInactiveBorder = CColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceInactiveBorder")->getValue()));

    Config::panelHeight = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelHeight")->getValue());
    Config::workspaceMargin = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceMargin")->getValue());
    Config::workspaceBorderSize = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceBorderSize")->getValue());
    Config::adaptiveHeight = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:adaptiveHeight")->getValue());
    Config::centerAligned = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:centerAligned")->getValue());
    Config::onTop = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:onTop")->getValue());
    Config::hideBackgroundLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideBackgroundLayers")->getValue());
    Config::drawActiveWorkspace = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:drawActiveWorkspace")->getValue());
    
    Config::overrideGaps = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:overrideGaps")->getValue());
    Config::gapsIn = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:gapsIn")->getValue());
    Config::gapsOut = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:gapsOut")->getValue());

    Config::autoDrag = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:autoDrag")->getValue());
    Config::autoScroll = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:autoScroll")->getValue());
    Config::exitOnClick = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:exitOnClick")->getValue());
    Config::switchOnDrop = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:switchOnDrop")->getValue());
    Config::exitOnSwitch = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:exitOnSwitch")->getValue());
    Config::showNewWorkspace = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:showNewWorkspace")->getValue());
    Config::showEmptyWorkspace = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:showEmptyWorkspace")->getValue());

    Config::dragAlpha = std::any_cast<Hyprlang::FLOAT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:dragAlpha")->getValue());

    Hyprlang::CConfigValue* numWorkspacesConfig = HyprlandAPI::getConfigValue(pHandle, "plugin:hyprsplit:num_workspaces");

    if (numWorkspacesConfig)
        hyprsplitNumWorkspaces = std::any_cast<Hyprlang::INT>(numWorkspacesConfig->getValue());

    // TODO: schedule frame for monitor?
}

void registerMonitors() {
    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (getWidgetForMonitor(m.get()) != nullptr) continue;
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.emplace_back(widget);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelColor", Hyprlang::INT{CColor(0,0,0,0).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBackground", Hyprlang::INT{CColor(0,0,0,0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBackground", Hyprlang::INT{CColor(0,0,0,0.5).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBorder", Hyprlang::INT{CColor(1,1,1,0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBorder", Hyprlang::INT{CColor(1,1,1,0).getAsHex()});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelHeight", Hyprlang::INT{250});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceMargin", Hyprlang::INT{12});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceBorderSize", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:adaptiveHeight", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:centerAligned", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:onTop", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideBackgroundLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:drawActiveWorkspace", Hyprlang::INT{1});
    
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:overrideGaps", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:gapsIn", Hyprlang::INT{20});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:gapsOut", Hyprlang::INT{60});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:autoDrag", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:autoScroll", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:exitOnClick", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:switchOnDrop", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:exitOnSwitch", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:showNewWorkspace", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:showEmptyWorkspace", Hyprlang::INT{1});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:dragAlpha", Hyprlang::FLOAT{0.2});

    reloadConfig();
    HyprlandAPI::registerCallbackDynamic(pHandle, "configReloaded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { reloadConfig(); });

    HyprlandAPI::addDispatcher(pHandle, "overview:toggle", dispatchToggleOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:open", dispatchOpenOverview);
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

    onAxisEventHook = HyprlandAPI::createFunctionHook(pHandle, findFunctionBySymbol(pHandle, "onAxisEvent", "CKeybindManager::onAxisEvent"), (void*)hkOnAxisEvent);
    if (onAxisEventHook)
        onAxisEventHook->hook();

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

    registerMonitors();
    HyprlandAPI::registerCallbackDynamic(pHandle, "monitorAdded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { registerMonitors(); });

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
    if (onAxisEventHook)
        onAxisEventHook->unhook();
    //if (glTexParameteriHook)
    //    glTexParameteriHook->unhook();
}