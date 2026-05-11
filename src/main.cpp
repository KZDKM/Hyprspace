#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include "Overview.hpp"
#include "Globals.hpp"

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

int numWorkspaces = -1; //hyprsplit/split-monitor-workspaces support

// Event listener handles (auto-unregister when destroyed)
CHyprSignalListener g_pRenderHook;
CHyprSignalListener g_pConfigReloadHook;
CHyprSignalListener g_pOpenLayerHook;
CHyprSignalListener g_pCloseLayerHook;
CHyprSignalListener g_pMouseButtonHook;
CHyprSignalListener g_pMouseAxisHook;
CHyprSignalListener g_pTouchDownHook;
CHyprSignalListener g_pTouchMoveHook;
CHyprSignalListener g_pTouchUpHook;
CHyprSignalListener g_pSwipeBeginHook;
CHyprSignalListener g_pSwipeUpdateHook;
CHyprSignalListener g_pSwipeEndHook;
CHyprSignalListener g_pKeyPressHook;
CHyprSignalListener g_pSwitchWorkspaceHook;
CHyprSignalListener g_pAddMonitorHook;

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

void onRender(eRenderStage renderStage) {

    // refresh layout after scheduled recalculation on monitors were carried out in renderMonitor
    if (renderStage == eRenderStage::RENDER_PRE) {
        if (g_layoutNeedsRefresh) {
            refreshWidgets();
            g_layoutNeedsRefresh = false;
        }
    }
    else if (renderStage == eRenderStage::RENDER_PRE_WINDOWS) {


        const auto widget = getWidgetForMonitor(g_pHyprRenderer->m_renderData.pMonitor);
        if (widget != nullptr)
            if (widget->getOwner()) {
                //widget->draw();
                const auto dragTarget = g_layoutManager->dragController()->target();
                const auto curWindow = dragTarget ? dragTarget->window() : nullptr;
                if (curWindow) {
                    if (widget->isActive()) {
                        g_oAlpha = curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->goal();
                        curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(0); // HACK: hide dragged window for the actual pass
                    }
                }
                else g_oAlpha = -1;
            }
            else g_oAlpha = -1;
        else g_oAlpha = -1;

    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOWS) {

        const auto widget = getWidgetForMonitor(g_pHyprRenderer->m_renderData.pMonitor);

        if (widget != nullptr)
            if (widget->getOwner()) {
                widget->draw();
                if (g_oAlpha != -1) {
                    const auto dragTarget = g_layoutManager->dragController()->target();
                    const auto curWindow = dragTarget ? dragTarget->window() : nullptr;
                    if (curWindow) {
                        curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(Config::dragAlpha);
                        curWindow->m_ruleApplicator->noBlur().unset(Desktop::Types::PRIORITY_SET_PROP);
                        const auto time = Time::steadyNow();
                        (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), curWindow, widget->getOwner(), time, true, Render::RENDER_PASS_MAIN, false, false);
                        curWindow->m_ruleApplicator->noBlur().unset(Desktop::Types::PRIORITY_SET_PROP);
                        curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(g_oAlpha);
                    }
                }
                g_oAlpha = -1;
            }

    }
}

// event hook, currently this is only here to re-hide top layer panels on workspace change
void onWorkspaceChange(PHLWORKSPACE pWorkspace) {

    if (!pWorkspace) return;

    auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromID(pWorkspace->m_monitor->m_id));
    if (widget != nullptr)
        if (widget->isActive())
            widget->show();
}

// event hook for click and drag interaction
void onMouseButton(const IPointer::SButtonEvent& event, SCallbackInfo& info) {
    const SP<IPointer> pointer = g_pSeatManager->m_mouse.lock();
    if (!pointer)
        return;

    if (event.button != BTN_LEFT) return;

    const auto pressed = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
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
void onMouseAxis(const IPointer::SAxisEvent& event, SCallbackInfo& info) {

    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                info.cancelled = !widget->axisEvent(event.delta, event.axis, g_pInputManager->getMouseCoordsInternal());
            }
        }
    }

}

// event hook for swipe
void onSwipeBegin(const IPointer::SSwipeBeginEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        widget->beginSwipe(event);

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
void onSwipeUpdate(const IPointer::SSwipeUpdateEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        info.cancelled = !widget->updateSwipe(event);
}

// event hook for end swipe
void onSwipeEnd(const IPointer::SSwipeEndEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        widget->endSwipe(event);
}

// Close overview with configurable key
void onKeyPress(const IKeyboard::SKeyEvent& event, SCallbackInfo& info) {
    const SP<IKeyboard> keyboard = g_pSeatManager->m_keyboard.lock();
    if (!keyboard || !keyboard->m_xkbSymState)
        return;

    const auto keycode = event.keycode + 8; // Because to xkbcommon it's +8 from libinput
    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->m_xkbSymState, keycode);

    auto* pExitKeyCfg = HyprlandAPI::getConfigValue(pHandle, "plugin:overview:exitKey");
    if (!pExitKeyCfg)
        return;

    const Hyprlang::STRING cfgExitKey = std::any_cast<Hyprlang::STRING>(pExitKeyCfg->getValue());
    if (!cfgExitKey || cfgExitKey[0] == '\0')
        return;

    const xkb_keysym_t cfgExitKeysym = xkb_keysym_from_name(cfgExitKey, XKB_KEYSYM_CASE_INSENSITIVE);

    if (keysym == cfgExitKeysym) {
        // close all panels
        bool overviewActive = false;
        for (auto& widget : g_overviewWidgets) {
            if (widget != nullptr && widget->isActive()) {
                widget->hide();
                overviewActive = true;
            }
        }
        // Only cancel event if overview was active and closed
        if (overviewActive)
            info.cancelled = true;
    }
}

PHLMONITOR g_pTouchedMonitor;

void onTouchDown(const ITouch::SDownEvent& event, SCallbackInfo& info) {
    if (!event.device)
        return;

    auto targetMonitor = g_pCompositor->getMonitorFromName(!event.device->m_boundOutput.empty() ? event.device->m_boundOutput : "");
    targetMonitor = targetMonitor ? targetMonitor : g_pCompositor->getMonitorFromCursor();

    const auto widget = getWidgetForMonitor(targetMonitor);
    if (widget != nullptr && targetMonitor != nullptr) {
        if (widget->isActive()) {
            Vector2D pos = targetMonitor->m_position + event.pos * targetMonitor->m_size;
            info.cancelled = !widget->buttonEvent(true, pos);
            if (info.cancelled) {
                g_pTouchedMonitor = targetMonitor;
                g_pCompositor->warpCursorTo(pos);
                g_pInputManager->refocus();
            }
        }
    }
}

void onTouchMove(const ITouch::SMotionEvent& event, SCallbackInfo& info) {
    if (g_pTouchedMonitor == nullptr) return;

    g_pCompositor->warpCursorTo(g_pTouchedMonitor->m_position + g_pTouchedMonitor->m_size * event.pos);
    g_pInputManager->simulateMouseMovement();
}

void onTouchUp(const ITouch::SUpEvent& event, SCallbackInfo& info) {
    const auto widget = getWidgetForMonitor(g_pTouchedMonitor);
    if (widget != nullptr && g_pTouchedMonitor != nullptr)
        if (widget->isActive())
            info.cancelled = !widget->buttonEvent(false, g_pInputManager->getMouseCoordsInternal());

    g_pTouchedMonitor = nullptr;
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
    
    // We don't need to store exitKey in Config namespace as it's only used in onKeyPress

    for (auto& widget : g_overviewWidgets) {
        widget->updateConfig();
        widget->hide();
        IPointer::SSwipeEndEvent dummy;
        dummy.cancelled = true;
        widget->endSwipe(dummy);
    }

    Config::dragAlpha = std::any_cast<Hyprlang::FLOAT>(HyprlandAPI::getConfigValue(pHandle, "plugin:overview:dragAlpha")->getValue());

    // get number of workspaces from hyprsplit or split-monitor-workspaces plugin config
    Hyprlang::CConfigValue* numWorkspacesConfig = HyprlandAPI::getConfigValue(pHandle, "plugin:hyprsplit:num_workspaces");
    if (!numWorkspacesConfig)
        numWorkspacesConfig = HyprlandAPI::getConfigValue(pHandle, "plugin:split-monitor-workspaces:count");
    if (numWorkspacesConfig)
        numWorkspaces = std::any_cast<Hyprlang::INT>(numWorkspacesConfig->getValue());

    // TODO: schedule frame for monitor?
}

void registerMonitors() {
    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_monitors) {
        if (getWidgetForMonitor(m) != nullptr) continue;
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->m_id);
        g_overviewWidgets.emplace_back(widget);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Log::logger->log(Log::DEBUG, "Loading overview plugin");

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

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:disableGestures", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:reverseSwipe", Hyprlang::INT{0});

    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:disableBlur", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:overrideAnimSpeed", Hyprlang::FLOAT{0.0});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:dragAlpha", Hyprlang::FLOAT{0.2});
    HyprlandAPI::addConfigValue(pHandle, "plugin:overview:exitKey", Hyprlang::STRING{"Escape"});

    g_pConfigReloadHook = Event::bus()->m_events.config.reloaded.listen([]() { reloadConfig(); });
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcherV2(pHandle, "overview:toggle", ::dispatchToggleOverview);
    HyprlandAPI::addDispatcherV2(pHandle, "overview:open", ::dispatchOpenOverview);
    HyprlandAPI::addDispatcherV2(pHandle, "overview:close", ::dispatchCloseOverview);

    g_pRenderHook = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) { onRender(stage); });

    // refresh on layer change
    g_pOpenLayerHook = Event::bus()->m_events.layer.opened.listen([](PHLLS) { g_layoutNeedsRefresh = true; });
    g_pCloseLayerHook = Event::bus()->m_events.layer.closed.listen([](PHLLS) { g_layoutNeedsRefresh = true; });


    g_pMouseButtonHook = listenCancellable<IPointer::SButtonEvent>(Event::bus()->m_events.input.mouse.button, onMouseButton);
    g_pMouseAxisHook = listenCancellable<IPointer::SAxisEvent>(Event::bus()->m_events.input.mouse.axis, onMouseAxis);

    g_pTouchDownHook = listenCancellable<ITouch::SDownEvent>(Event::bus()->m_events.input.touch.down, onTouchDown);
    g_pTouchMoveHook = listenCancellable<ITouch::SMotionEvent>(Event::bus()->m_events.input.touch.motion, onTouchMove);
    g_pTouchUpHook = listenCancellable<ITouch::SUpEvent>(Event::bus()->m_events.input.touch.up, onTouchUp);

    g_pSwipeBeginHook = listenCancellable<IPointer::SSwipeBeginEvent>(Event::bus()->m_events.gesture.swipe.begin, onSwipeBegin);
    g_pSwipeUpdateHook = listenCancellable<IPointer::SSwipeUpdateEvent>(Event::bus()->m_events.gesture.swipe.update, onSwipeUpdate);
    g_pSwipeEndHook = listenCancellable<IPointer::SSwipeEndEvent>(Event::bus()->m_events.gesture.swipe.end, onSwipeEnd);

    g_pKeyPressHook = listenCancellable<IKeyboard::SKeyEvent>(Event::bus()->m_events.input.keyboard.key, onKeyPress);

    g_pSwitchWorkspaceHook = Event::bus()->m_events.workspace.active.listen(onWorkspaceChange);

    pRenderWindow = findFunctionBySymbol(pHandle, "renderWindow", "IHyprRenderer::renderWindow");
    if (!pRenderWindow)
        pRenderWindow = findFunctionBySymbol(pHandle, "renderWindow", "CHyprRenderer::renderWindow");
    pRenderLayer = findFunctionBySymbol(pHandle, "renderLayer", "IHyprRenderer::renderLayer");
    if (!pRenderLayer)
        pRenderLayer = findFunctionBySymbol(pHandle, "renderLayer", "CHyprRenderer::renderLayer");

    registerMonitors();
    g_pAddMonitorHook = Event::bus()->m_events.monitor.added.listen([](PHLMONITOR) { registerMonitors(); });

    return {"Hyprspace", "Workspace overview", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pRenderHook.reset();
    g_pConfigReloadHook.reset();
    g_pOpenLayerHook.reset();
    g_pCloseLayerHook.reset();
    g_pMouseButtonHook.reset();
    g_pMouseAxisHook.reset();
    g_pTouchDownHook.reset();
    g_pTouchMoveHook.reset();
    g_pTouchUpHook.reset();
    g_pSwipeBeginHook.reset();
    g_pSwipeUpdateHook.reset();
    g_pSwipeEndHook.reset();
    g_pKeyPressHook.reset();
    g_pSwitchWorkspaceHook.reset();
    g_pAddMonitorHook.reset();

    g_overviewWidgets.clear();

    pRenderWindow = nullptr;
    pRenderLayer = nullptr;
    pHandle = nullptr;
}
