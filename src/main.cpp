#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <any>
#include "Overview.hpp"
#include "Globals.hpp"
#include "Lua.hpp"

void* pRenderWindow;
void* pRenderLayer;

std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;

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
CHyprSignalListener g_pStartHook;

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
                        curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(config.dragAlpha->value());
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

    auto widget = getWidgetForMonitor(State::monitorState()->query().id(pWorkspace->m_monitor->m_id).run());
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
    const auto pMonitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
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

    const auto pMonitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
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

    if (config.disableGestures->value()) return;

    const auto widget = getWidgetForMonitor(State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run());
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

    if (config.disableGestures->value()) return;

    const auto widget = getWidgetForMonitor(State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run());
    if (widget != nullptr)
        info.cancelled = !widget->updateSwipe(event);
}

// event hook for end swipe
void onSwipeEnd(const IPointer::SSwipeEndEvent& event, SCallbackInfo& info) {

    if (config.disableGestures->value()) return;

    const auto widget = getWidgetForMonitor(State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run());
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

    const std::string cfgExitKey = config.exitKey->value();
    if (cfgExitKey.empty())
        return;

    const xkb_keysym_t cfgExitKeysym = xkb_keysym_from_name(cfgExitKey.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

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

    auto targetMonitor = State::monitorState()->query().name(!event.device->m_boundOutput.empty() ? event.device->m_boundOutput : "").run();
    targetMonitor = targetMonitor ? targetMonitor : State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();

    const auto widget = getWidgetForMonitor(targetMonitor);
    if (widget != nullptr && targetMonitor != nullptr) {
        if (widget->isActive()) {
            Vector2D pos = targetMonitor->m_position + event.pos * targetMonitor->m_size;
            info.cancelled = !widget->buttonEvent(true, pos);
            if (info.cancelled) {
                g_pTouchedMonitor = targetMonitor;
                Pointer::pointerController()->warpTo(pos);
                g_pInputManager->refocus();
            }
        }
    }
}

void onTouchMove(const ITouch::SMotionEvent& event, SCallbackInfo& info) {
    if (g_pTouchedMonitor == nullptr) return;

    Pointer::pointerController()->warpTo(g_pTouchedMonitor->m_position + g_pTouchedMonitor->m_size * event.pos);
    g_pInputManager->simulateMouseMovement();
}

void onTouchUp(const ITouch::SUpEvent& event, SCallbackInfo& info) {
    const auto widget = getWidgetForMonitor(g_pTouchedMonitor);
    if (widget != nullptr && g_pTouchedMonitor != nullptr)
        if (widget->isActive())
            info.cancelled = !widget->buttonEvent(false, g_pInputManager->getMouseCoordsInternal());

    g_pTouchedMonitor = nullptr;
}

SDispatchResult Dispatchers::dispatchToggleOverview(std::string arg) {
    auto currentMonitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
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

SDispatchResult Dispatchers::dispatchOpenOverview(std::string arg) {
    if (arg.contains("all")) {
        for (auto& widget : g_overviewWidgets) {
            if (!widget->isActive()) widget->show();
        }
    }
    else {
        auto currentMonitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
        auto widget = getWidgetForMonitor(currentMonitor);
        if (widget)
            if (!widget->isActive()) widget->show();
    }
    return SDispatchResult{};
}

SDispatchResult Dispatchers::dispatchCloseOverview(std::string arg) {
    if (arg.contains("all")) {
        for (auto& widget : g_overviewWidgets) {
            if (widget->isActive()) widget->hide();
        }
    }
    else {
        auto currentMonitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
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
    for (auto& widget : g_overviewWidgets) {
        widget->updateConfig();
        if (widget->isActive() || widget->isSwiping()) {
            widget->hide();
            IPointer::SSwipeEndEvent dummy;
            dummy.cancelled = true;
            widget->endSwipe(dummy);
        }
    }

    // get number of workspaces from hyprsplit or split-monitor-workspaces plugin config
    numWorkspaces = -1;
    auto numWorkspacesOpt = HyprConfig::getHyprsplitNumWorkspaces();
    if (!numWorkspacesOpt.has_value())
        numWorkspacesOpt = HyprConfig::getSplitMonitorWorkspacesCount();
    if (numWorkspacesOpt.has_value())
        numWorkspaces = numWorkspacesOpt.value();

    // TODO: schedule frame for monitor?
}

void registerMonitors() {
    // create a widget for each monitor
    for (auto& m : State::monitorState()->monitors()) {
        if (getWidgetForMonitor(m) != nullptr) continue;
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->m_id);
        g_overviewWidgets.emplace_back(widget);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Log::logger->log(Log::DEBUG, "Loading overview plugin");

    HyprlandAPI::addConfigValueV2(pHandle, config.panelBaseColor);
    HyprlandAPI::addConfigValueV2(pHandle, config.panelBorderColor);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceActiveBackground);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceInactiveBackground);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceActiveBorder);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceInactiveBorder);

    HyprlandAPI::addConfigValueV2(pHandle, config.panelHeight);
    HyprlandAPI::addConfigValueV2(pHandle, config.panelBorderWidth);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceMargin);
    HyprlandAPI::addConfigValueV2(pHandle, config.workspaceBorderSize);
    HyprlandAPI::addConfigValueV2(pHandle, config.reservedArea);
    HyprlandAPI::addConfigValueV2(pHandle, config.adaptiveHeight);
    HyprlandAPI::addConfigValueV2(pHandle, config.centerAligned);
    HyprlandAPI::addConfigValueV2(pHandle, config.onBottom);
    HyprlandAPI::addConfigValueV2(pHandle, config.hideBackgroundLayers);
    HyprlandAPI::addConfigValueV2(pHandle, config.hideTopLayers);
    HyprlandAPI::addConfigValueV2(pHandle, config.hideOverlayLayers);
    HyprlandAPI::addConfigValueV2(pHandle, config.drawActiveWorkspace);
    HyprlandAPI::addConfigValueV2(pHandle, config.hideRealLayers);
    HyprlandAPI::addConfigValueV2(pHandle, config.affectStrut);

    HyprlandAPI::addConfigValueV2(pHandle, config.overrideGaps);
    HyprlandAPI::addConfigValueV2(pHandle, config.gapsIn);
    HyprlandAPI::addConfigValueV2(pHandle, config.gapsOut);

    HyprlandAPI::addConfigValueV2(pHandle, config.autoDrag);
    HyprlandAPI::addConfigValueV2(pHandle, config.autoScroll);
    HyprlandAPI::addConfigValueV2(pHandle, config.exitOnClick);
    HyprlandAPI::addConfigValueV2(pHandle, config.switchOnDrop);
    HyprlandAPI::addConfigValueV2(pHandle, config.exitOnSwitch);
    HyprlandAPI::addConfigValueV2(pHandle, config.showNewWorkspace);
    HyprlandAPI::addConfigValueV2(pHandle, config.showEmptyWorkspace);
    HyprlandAPI::addConfigValueV2(pHandle, config.showSpecialWorkspace);

    HyprlandAPI::addConfigValueV2(pHandle, config.disableGestures);
    HyprlandAPI::addConfigValueV2(pHandle, config.reverseSwipe);

    HyprlandAPI::addConfigValueV2(pHandle, config.disableBlur);
    HyprlandAPI::addConfigValueV2(pHandle, config.overrideAnimSpeed);
    HyprlandAPI::addConfigValueV2(pHandle, config.dragAlpha);
    HyprlandAPI::addConfigValueV2(pHandle, config.exitKey);

    g_pConfigReloadHook = Event::bus()->m_events.config.reloaded.listen([]() { reloadConfig(); });
    g_pStartHook = Event::bus()->m_events.start.listen([]() {
        reloadConfig();
        registerMonitors();
    });
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcherV2(pHandle, "overview:toggle", Dispatchers::dispatchToggleOverview);
    HyprlandAPI::addDispatcherV2(pHandle, "overview:open", Dispatchers::dispatchOpenOverview);
    HyprlandAPI::addDispatcherV2(pHandle, "overview:close", Dispatchers::dispatchCloseOverview);

    registerLuaBindings(pHandle);

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
    g_pStartHook.reset();

    g_overviewWidgets.clear();

    pRenderWindow = nullptr;
    pRenderLayer = nullptr;
    pHandle = nullptr;
}
