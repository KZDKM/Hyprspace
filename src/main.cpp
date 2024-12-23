#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include "Overview.hpp"
#include "Globals.hpp"

void* pMouseKeybind;
void* pRenderWindow;
void* pRenderLayer;

std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;


CHyprColor Config::panelBaseColor = CHyprColor(0, 0, 0, 0);
CHyprColor Config::panelBorderColor = CHyprColor(0, 0, 0, 0);
CHyprColor Config::workspaceActiveBackground = CHyprColor(0, 0, 0, 0.25);
CHyprColor Config::workspaceInactiveBackground = CHyprColor(0, 0, 0, 0.5);
CHyprColor Config::workspaceActiveBorder = CHyprColor(1, 1, 1, 0.3);
CHyprColor Config::workspaceInactiveBorder = CHyprColor(1, 1, 1, 0);

int Config::panelHeight = 250;
int Config::panelBorderWidth = 2;
int Config::workspaceMargin = 12;
int Config::reservedArea = 0;
int Config::workspaceBorderSize = 1;
bool Config::adaptiveHeight = false; // TODO: implement
bool Config::centerAligned = true;
bool Config::onBottom = true; // TODO: implement
bool Config::hideBackgroundLayers = false;
bool Config::hideTopLayers = false;
bool Config::hideOverlayLayers = false;
bool Config::drawActiveWorkspace = true;
bool Config::hideRealLayers = true;
bool Config::affectStrut = true;

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
bool Config::reverseSwipe = false;

bool Config::disableBlur = false;

float Config::overrideAnimSpeed = 0;

float Config::dragAlpha = 0.2;

int hyprsplitNumWorkspaces = -1;

Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pRenderHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pConfigReloadHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pOpenLayerHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pCloseLayerHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pMouseButtonHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pMouseAxisHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pTouchDownHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pTouchUpHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pSwipeBeginHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pSwipeUpdateHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pSwipeEndHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pKeyPressHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pSwitchWorkspaceHook;
Hyprutils::Memory::CSharedPointer<HOOK_CALLBACK_FN> g_pAddMonitorHook;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

std::shared_ptr<CHyprspaceWidget> getWidgetForMonitor(PHLMONITORREF pMonitor) {
    for (auto& widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            return widget;
        }
    }
    return nullptr;
}

// used to enforce the layout
void refreshWidgets() {
    for (auto& widget : g_overviewWidgets) {
        if (widget != nullptr)
            if (widget->isActive())
                widget->show();
    }
}

bool g_layoutNeedsRefresh = true;

// for restroing dragged window's alpha value
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
        if (widget != nullptr)
            if (widget->getOwner()) {
                //widget->draw();
                if (const auto curWindow = g_pInputManager->currentlyDraggedWindow.lock()) {
                    if (widget->isActive()) {
                        g_oAlpha = curWindow->m_fActiveInactiveAlpha.goal();
                        curWindow->m_fActiveInactiveAlpha.setValueAndWarp(0); // HACK: hide dragged window for the actual pass
                    }
                }
                else g_oAlpha = -1;
            }
            else g_oAlpha = -1;
        else g_oAlpha = -1;

    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOWS) {

        const auto widget = getWidgetForMonitor(g_pHyprOpenGL->m_RenderData.pMonitor);

        if (widget != nullptr)
            if (widget->getOwner()) {
                widget->draw();
                if (g_oAlpha != -1) {
                    if (const auto curWindow = g_pInputManager->currentlyDraggedWindow.lock()) {
                        curWindow->m_fActiveInactiveAlpha.setValueAndWarp(Config::dragAlpha);
                        timespec time;
                        clock_gettime(CLOCK_MONOTONIC, &time);
                        (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), curWindow, widget->getOwner(), &time, true, RENDER_PASS_MAIN, false, false);
                        curWindow->m_fActiveInactiveAlpha.setValueAndWarp(g_oAlpha);
                    }
                }
                g_oAlpha = -1;
            }

    }
}

// event hook, currently this is only here to re-hide top layer panels on workspace change
void onWorkspaceChange(void* thisptr, SCallbackInfo& info, std::any args) {

    // wiki is outdated, this is PHLWORKSPACE rather than CWorkspace*
    const auto pWorkspace = std::any_cast<PHLWORKSPACE>(args);
    if (!pWorkspace) return;

    auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromID(pWorkspace->m_pMonitor->ID));
    if (widget != nullptr)
        if (widget->isActive())
            widget->show();
}

// event hook for click and drag interaction
void onMouseButton(void* thisptr, SCallbackInfo& info, std::any args) {

    const auto e = std::any_cast<IPointer::SButtonEvent>(args);

    if (e.button != BTN_LEFT) return;

    const auto pressed = e.state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                info.cancelled = !widget->buttonEvent(pressed, g_pInputManager->getMouseCoordsInternal());
            }
        }
    }

}

// event hook for scrolling through panel and workspaces
void onMouseAxis(void* thisptr, SCallbackInfo& info, std::any args) {

    const auto e = std::any_cast<IPointer::SAxisEvent>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["event"]);

    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL)
                    info.cancelled = !widget->axisEvent(e.delta, g_pInputManager->getMouseCoordsInternal());
            }
        }
    }

}

// event hook for swipe
void onSwipeBegin(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<IPointer::SSwipeBeginEvent>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        widget->beginSwipe(e);

    // end other widget swipe
    for (auto& w : g_overviewWidgets) {
        if (w != widget && w->isSwiping()) {
            IPointer::SSwipeEndEvent dummy;
            dummy.cancelled = true;
            w->endSwipe(dummy);
        }
    }
}

// event hook for update swipe, most of the swiping mechanics are here
void onSwipeUpdate(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<IPointer::SSwipeUpdateEvent>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        info.cancelled = !widget->updateSwipe(e);
}

// event hook for end swipe
void onSwipeEnd(void* thisptr, SCallbackInfo& info, std::any args) {

    if (Config::disableGestures) return;

    const auto e = std::any_cast<IPointer::SSwipeEndEvent>(args);

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        widget->endSwipe(e);
}

// atm this is only for ESC to exit
void onKeyPress(void* thisptr, SCallbackInfo& info, std::any args) {
    const auto e = std::any_cast<IKeyboard::SKeyEvent>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["event"]);
    //const auto k = std::any_cast<SKeyboard*>(std::any_cast<std::unordered_map<std::string, std::any>>(args)["keyboard"]);

    if (e.keycode == KEY_ESC) {
        // close all panels
        for (auto& widget : g_overviewWidgets) {
            if (widget != nullptr)
                if (widget->isActive()) {
                    widget->hide();
                    info.cancelled = true;
                }
        }
    }
}

void onTouchDown(void* thisptr, SCallbackInfo& info, std::any args) {
    const auto e = std::any_cast<ITouch::SDownEvent>(args);
    const auto targetMonitor = g_pCompositor->getMonitorFromName(e.device ? e.device->deviceName : "");
    const auto widget = getWidgetForMonitor(targetMonitor);
    if (widget != nullptr && targetMonitor != nullptr)
        if (widget->isActive())
            info.cancelled = !widget->buttonEvent(true, { targetMonitor->vecPosition.x + e.pos.x * targetMonitor->vecSize.x, targetMonitor->vecPosition.y + e.pos.y * targetMonitor->vecSize.y });
}

void onTouchUp(void* thisptr, SCallbackInfo& info, std::any args) {
    const auto e = std::any_cast<ITouch::SUpEvent>(args);
    const auto targetMonitor = g_pCompositor->getMonitorFromID(e.touchID);
    const auto widget = getWidgetForMonitor(targetMonitor);
    if (widget != nullptr && targetMonitor != nullptr)
        if (widget->isActive())
            info.cancelled = !widget->buttonEvent(false, g_pInputManager->getMouseCoordsInternal());
}

static SDispatchResult dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget) {
        if (arg.contains("all")) {
            if (widget->isActive()) {
                for (auto& widget : g_overviewWidgets) {
                    if (widget != nullptr)
                        if (widget->isActive())
                            widget->hide();
                }
            }
            else {
                for (auto& widget : g_overviewWidgets) {
                    if (widget != nullptr)
                        if (!widget->isActive())
                            widget->show();
                }
            }
        }
        else
            widget->isActive() ? widget->hide() : widget->show();
    }
    return SDispatchResult{};
}

static SDispatchResult dispatchOpenOverview(std::string arg) {
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
    return SDispatchResult{};
}

static SDispatchResult dispatchCloseOverview(std::string arg) {
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
    return SDispatchResult{};
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
    Config::panelBaseColor = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelColor")->getValue()));
    Config::panelBorderColor = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelBorderColor")->getValue()));
    Config::workspaceActiveBackground = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceActiveBackground")->getValue()));
    Config::workspaceInactiveBackground = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceInactiveBackground")->getValue()));
    Config::workspaceActiveBorder = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceActiveBorder")->getValue()));
    Config::workspaceInactiveBorder = CHyprColor(std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceInactiveBorder")->getValue()));

    Config::panelHeight = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelHeight")->getValue());
    Config::panelBorderWidth = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:panelBorderWidth")->getValue());
    Config::workspaceMargin = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceMargin")->getValue());
    Config::reservedArea = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:reservedArea")->getValue());
    Config::workspaceBorderSize = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:workspaceBorderSize")->getValue());
    Config::adaptiveHeight = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:adaptiveHeight")->getValue());
    Config::centerAligned = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:centerAligned")->getValue());
    Config::onBottom = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:onBottom")->getValue());
    Config::hideBackgroundLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideBackgroundLayers")->getValue());
    Config::hideTopLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideTopLayers")->getValue());
    Config::hideOverlayLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideOverlayLayers")->getValue());
    Config::drawActiveWorkspace = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:drawActiveWorkspace")->getValue());
    Config::hideRealLayers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:hideRealLayers")->getValue());
    Config::affectStrut = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:affectStrut")->getValue());

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
    Config::reverseSwipe = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:reverseSwipe")->getValue());

    Config::disableBlur = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:disableBlur")->getValue());

    Config::overrideAnimSpeed = std::any_cast<Hyprlang::FLOAT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:overrideAnimSpeed")->getValue());

    for (auto& widget : g_overviewWidgets) {
        widget->updateConfig();
        widget->hide();
        IPointer::SSwipeEndEvent dummy;
        dummy.cancelled = true;
        widget->endSwipe(dummy);
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
        if (getWidgetForMonitor(m) != nullptr) continue;
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->ID);
        g_overviewWidgets.emplace_back(widget);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Debug::log(LOG, "Loading overview plugin");

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelColor", Hyprlang::INT{CHyprColor(0, 0, 0, 0).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelBorderColor", Hyprlang::INT{CHyprColor(0, 0, 0, 0).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBackground", Hyprlang::INT{CHyprColor(0, 0, 0, 0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBackground", Hyprlang::INT{CHyprColor(0, 0, 0, 0.5).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceActiveBorder", Hyprlang::INT{CHyprColor(1, 1, 1, 0.25).getAsHex()});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceInactiveBorder", Hyprlang::INT{CHyprColor(1, 1, 1, 0).getAsHex()});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelHeight", Hyprlang::INT{250});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:panelBorderWidth", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceMargin", Hyprlang::INT{12});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:workspaceBorderSize", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:reservedArea", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:adaptiveHeight", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:centerAligned", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:onBottom", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideBackgroundLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideTopLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideOverlayLayers", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:drawActiveWorkspace", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:hideRealLayers", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:affectStrut", Hyprlang::INT{1});

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

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:disableGestures", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:reverseSwipe", Hyprlang::INT{0});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:disableBlur", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:overrideAnimSpeed", Hyprlang::FLOAT{0.0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:dragAlpha", Hyprlang::FLOAT{0.2});

    g_pConfigReloadHook = HyprlandAPI::registerCallbackDynamic(pHandle, "configReloaded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { reloadConfig(); });
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcher(pHandle, "overview:toggle", ::dispatchToggleOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:open", ::dispatchOpenOverview);
    HyprlandAPI::addDispatcher(pHandle, "overview:close", ::dispatchCloseOverview);

    g_pRenderHook = HyprlandAPI::registerCallbackDynamic(pHandle, "render", onRender);

    // refresh on layer change
    g_pOpenLayerHook = HyprlandAPI::registerCallbackDynamic(pHandle, "openLayer", [&] (void* thisptr, SCallbackInfo& info, std::any data) { g_layoutNeedsRefresh = true; });
    g_pCloseLayerHook = HyprlandAPI::registerCallbackDynamic(pHandle, "closeLayer", [&] (void* thisptr, SCallbackInfo& info, std::any data) { g_layoutNeedsRefresh = true; });


    // CKeybindManager::mouse (names too generic bruh) (this is a private function btw)
    pMouseKeybind = findFunctionBySymbol(pHandle, "mouse", "CKeybindManager::mouse");

    g_pMouseButtonHook = HyprlandAPI::registerCallbackDynamic(pHandle, "mouseButton", onMouseButton);
    g_pMouseAxisHook = HyprlandAPI::registerCallbackDynamic(pHandle, "mouseAxis", onMouseAxis);

    g_pTouchDownHook = HyprlandAPI::registerCallbackDynamic(pHandle, "touchDown", onTouchDown);
    g_pTouchUpHook = HyprlandAPI::registerCallbackDynamic(pHandle, "touchUp", onTouchUp);

    g_pSwipeBeginHook = HyprlandAPI::registerCallbackDynamic(pHandle, "swipeBegin", onSwipeBegin);
    g_pSwipeUpdateHook = HyprlandAPI::registerCallbackDynamic(pHandle, "swipeUpdate", onSwipeUpdate);
    g_pSwipeEndHook = HyprlandAPI::registerCallbackDynamic(pHandle, "swipeEnd", onSwipeEnd);

    g_pKeyPressHook = HyprlandAPI::registerCallbackDynamic(pHandle, "keyPress", onKeyPress);

    g_pSwitchWorkspaceHook = HyprlandAPI::registerCallbackDynamic(pHandle, "workspace", onWorkspaceChange);

    // CHyprRenderer::renderWindow
    auto funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderWindow");
    pRenderWindow = funcSearch[0].address;

    // CHyprRenderer::renderLayer
    funcSearch = HyprlandAPI::findFunctionsByName(pHandle, "renderLayer");
    pRenderLayer = funcSearch[0].address;

    registerMonitors();
    g_pAddMonitorHook = HyprlandAPI::registerCallbackDynamic(pHandle, "monitorAdded", [&] (void* thisptr, SCallbackInfo& info, std::any data) { registerMonitors(); });

    return {"Hyprspace", "Workspace overview", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
}
