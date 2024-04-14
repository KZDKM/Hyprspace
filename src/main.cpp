#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include "Overview.hpp"
#include "Globals.hpp"

using namespace std;

CFunctionHook* renderWorkspaceWindowsHook;
CFunctionHook* getWorkspaceRuleForHook;
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
int Config::reservedArea = 0;
int Config::workspaceBorderSize = 1;
bool Config::adaptiveHeight = false; // TODO: implement
bool Config::centerAligned = true;
bool Config::onTop = true; // TODO: implement
bool Config::hideBackgroundLayers = false;
bool Config::hideTopLayers = false;
bool Config::hideOverlayLayers = false;
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
bool Config::showSpecialWorkspace = false;

bool Config::disableGestures = false;

float Config::overrideAnimSpeed = 0;

float Config::dragAlpha = 0.2;

int hyprsplitNumWorkspaces = -1;


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

void refreshWidgets() {
    for (auto& widget : g_overviewWidgets) {
        if (widget.get())
            if (widget->isActive())
                widget->show();
    }
}

bool g_layoutNeedsRefresh = true;
float g_oAlpha = -1;
void onRender(void* thisptr, SCallbackInfo& info, std::any args) {

    const auto renderStage = std::any_cast<eRenderStage>(args);

    // refresh layout after scheduled recalculation on monitors were carried out in renderMonitor
    if (renderStage == eRenderStage::RENDER_PRE) {
        if (g_layoutNeedsRefresh) {
            refreshWidgets();
            g_layoutNeedsRefresh = false;
        }
    }
    else if (renderStage == eRenderStage::RENDER_PRE_WINDOWS) {


        const auto widget = getWidgetForMonitor(g_pHyprOpenGL->m_RenderData.pMonitor);
        if (widget.get())
            if (widget->getOwner()) {
                widget->draw();
                if (g_pInputManager->currentlyDraggedWindow && widget->isActive()) {
                    g_oAlpha = g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.goal();
                    g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.setValueAndWarp(Config::dragAlpha);
                }
                else g_oAlpha = -1;
            }
            else g_oAlpha = -1;
        else g_oAlpha = -1;

    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOWS) {
        if (g_oAlpha != -1 && g_pInputManager->currentlyDraggedWindow) {
            g_pInputManager->currentlyDraggedWindow->m_fActiveInactiveAlpha.setValueAndWarp(g_oAlpha);
        }
        g_oAlpha = -1;
    }
}

// event hook, currently this is only here to re-hide top layer panels on workspace change
void onWorkspaceChange(void* thisptr, SCallbackInfo& info, std::any args) {

    // wiki is outdated, this is PHLWORKSPACE rather than CWorkspace*
    const auto pWorkspace = std::any_cast<PHLWORKSPACE>(args);
    if (!pWorkspace) return;

    auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID));
    if (widget.get())
        if (widget->isActive())
            widget->show();
}

// event hook for click and drag interaction
void onMouseButton(void* thisptr, SCallbackInfo& info, std::any args) {

    const auto e = std::any_cast<wlr_pointer_button_event*>(args);
    if (!e) return;

    const auto pressed = e->state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                info.cancelled = !widget->buttonEvent(pressed);
            }
        }
    }

}

// event hook for scrolling through panel and workspaces
void onMouseAxis(void* thisptr, SCallbackInfo& info, std::any args) {

    const auto e = std::any_cast<wlr_pointer_axis_event*>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["event"]);
    if (!e) return;

    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                if (e->source == WL_POINTER_AXIS_SOURCE_WHEEL && e->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
                    info.cancelled = !widget->axisEvent(e->delta);
            }
        }
    }

}

void onSwipeBegin(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<wlr_pointer_swipe_begin_event*>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget.get())
        widget->beginSwipe(e);

    // end other widget swipe
    for (auto& w : g_overviewWidgets) {
        if (w != widget && w->isSwiping()) {
            w->endSwipe(0);
        }
    }
}

void onSwipeUpdate(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<wlr_pointer_swipe_update_event*>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget.get())
        info.cancelled = !widget->updateSwipe(e);
}

void onSwipeEnd(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<wlr_pointer_swipe_end_event*>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget.get())
        widget->endSwipe(e);
}

void onKeyPress(void* thisptr, SCallbackInfo& info, std::any args) {
    const auto e = std::any_cast<wlr_keyboard_key_event*>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["event"]);
    const auto k = std::any_cast<SKeyboard*>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["keyboard"]);

    if (e->keycode == KEY_ESC) {
        const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
        if (widget.get())
            if (widget->isActive()) {
                widget->hide();
                info.cancelled = true;
            }
    }
}

void dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget) {
        if (arg.contains("all")) {
            if (widget->isActive()) {
                for (auto& widget : g_overviewWidgets) {
                    if (widget.get())
                        if (widget->isActive()) 
                            widget->hide();
                }
            }
            else {
                for (auto& widget : g_overviewWidgets) {
                    if (widget.get())
                        if (!widget->isActive()) 
                            widget->show();
                }
            }
        } else
            widget->isActive() ? widget->hide() : widget->show();
    }
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
    Config::reservedArea = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:reservedArea")->getValue());
    Config::workspaceBorderSize = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceBorderSize")->getValue());
    Config::adaptiveHeight = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:adaptiveHeight")->getValue());
    Config::centerAligned = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:centerAligned")->getValue());
    Config::onTop = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:onTop")->getValue());
    Config::hideBackgroundLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideBackgroundLayers")->getValue());
    Config::hideTopLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideTopLayers")->getValue());
    Config::hideOverlayLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideOverlayLayers")->getValue());
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
    Config::showSpecialWorkspace = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:showSpecialWorkspace")->getValue());

    Config::disableGestures = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:disableGestures")->getValue());

    Config::overrideAnimSpeed = std::any_cast<Hyprlang::FLOAT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:overrideAnimSpeed")->getValue());

    for (auto& widget : g_overviewWidgets) {
        widget->updateConfig();
        widget->hide();
        widget->endSwipe(0);
    }

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

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelColor", Hyprlang::INT{CColor(0, 0, 0, 0).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBackground", Hyprlang::INT{CColor(0, 0, 0, 0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBackground", Hyprlang::INT{CColor(0, 0, 0, 0.5).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBorder", Hyprlang::INT{CColor(1, 1, 1, 0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBorder", Hyprlang::INT{CColor(1, 1, 1, 0).getAsHex()});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelHeight", Hyprlang::INT{250});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceMargin", Hyprlang::INT{12});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceBorderSize", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:reservedArea", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:adaptiveHeight", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:centerAligned", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:onTop", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideBackgroundLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideTopLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideOverlayLayers", Hyprlang::INT{0});
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
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:showSpecialWorkspace", Hyprlang::INT{0});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:disableGestures", Hyprlang::INT{1});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:overrideAnimSpeed", Hyprlang::FLOAT{0.0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:dragAlpha", Hyprlang::FLOAT{0.2});

    HyprlandAPI::registerCallbackDynamic(pHandle, "configReloaded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { reloadConfig(); });
    reloadConfig();

    HyprlandAPI::addDispatcher(pHandle, "overview:toggle", dispatchToggleOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:open", dispatchOpenOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:close", dispatchCloseOverview);

    HyprlandAPI::registerCallbackDynamic(pHandle, "render", onRender);

    // refresh on layer change
    HyprlandAPI::registerCallbackDynamic(pHandle, "openLayer", [&] (void* thisptr, SCallbackInfo& info, std::any data) { g_layoutNeedsRefresh = true; });
    HyprlandAPI::registerCallbackDynamic(pHandle, "closeLayer", [&] (void* thisptr, SCallbackInfo& info, std::any data) { g_layoutNeedsRefresh = true; });


    // CKeybindManager::mouse (names too generic bruh) (this is a private function btw)
    pMouseKeybind = findFunctionBySymbol(pHandle, "mouse", "CKeybindManager::mouse");

    HyprlandAPI::registerCallbackDynamic(pHandle, "mouseButton", onMouseButton);
    HyprlandAPI::registerCallbackDynamic(pHandle, "mouseAxis", onMouseAxis);

    HyprlandAPI::registerCallbackDynamic(pHandle, "swipeBegin", onSwipeBegin);
    HyprlandAPI::registerCallbackDynamic(pHandle, "swipeUpdate", onSwipeUpdate);
    HyprlandAPI::registerCallbackDynamic(pHandle, "swipeEnd", onSwipeEnd);

    HyprlandAPI::registerCallbackDynamic(pHandle, "keyPress", onKeyPress);

    HyprlandAPI::registerCallbackDynamic(pHandle, "workspace", onWorkspaceChange);

    // CHyprRenderer::renderWindow
    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWindow");
    pRenderWindow = funcSearch[0].address;

    // CHyprRenderer::renderLayer
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderLayer");
    pRenderLayer = funcSearch[0].address;

    registerMonitors();
    HyprlandAPI::registerCallbackDynamic(pHandle, "monitorAdded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { registerMonitors(); });

    return {"Hyprspace", "Workspace overview", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
}