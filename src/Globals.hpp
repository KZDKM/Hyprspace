#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/types.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <optional>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

// Hyprland v0.54+: cancellable input uses Event::SCallbackInfo (not legacy CEvent*).
using SCallbackInfo = Event::SCallbackInfo;

// Must match Hyprutils::Signal::CSignalT::RefArg (hyprutils/signal/Signal.hpp).
template <typename T>
using HyprSignalRefArg = std::conditional_t<std::is_trivially_copyable_v<T>, T, const T&>;

// Unpack Hyprutils::CSignalT::emit() tuple — first event arg is often stored by value (trivial types).
template <typename EventType, typename Signal>
CHyprSignalListener listenCancellable(Signal& signal, std::function<void(const EventType&, SCallbackInfo&)> handler) {
    struct Hack : Hyprutils::Signal::CSignalBase {
        using CSignalBase::registerListenerInternal;
    };
    return reinterpret_cast<Hack&>(signal).registerListenerInternal([handler](void* args) {
        using Tuple = std::tuple<HyprSignalRefArg<EventType>, HyprSignalRefArg<Event::SCallbackInfo&>>;
        auto* tup = static_cast<Tuple*>(args);
        handler(std::get<0>(*tup), std::get<1>(*tup));
    });
}

inline HANDLE pHandle = NULL;

typedef void (*tRenderWindow)(void*, PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, Render::eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, PHLLS, PHLMONITOR, const Time::steady_tp&, bool, bool);
extern void* pRenderLayer;

struct SConfig {
    SP<Config::Values::CColorValue> panelBaseColor;
    SP<Config::Values::CColorValue> panelBorderColor;
    SP<Config::Values::CColorValue> workspaceActiveBackground;
    SP<Config::Values::CColorValue> workspaceInactiveBackground;
    SP<Config::Values::CColorValue> workspaceActiveBorder;
    SP<Config::Values::CColorValue> workspaceInactiveBorder;

    SP<Config::Values::CIntValue> panelHeight;
    SP<Config::Values::CIntValue> panelBorderWidth;
    SP<Config::Values::CIntValue> workspaceMargin;
    SP<Config::Values::CIntValue> workspaceBorderSize;
    SP<Config::Values::CIntValue> reservedArea;
    SP<Config::Values::CIntValue> adaptiveHeight;
    SP<Config::Values::CIntValue> centerAligned;
    SP<Config::Values::CIntValue> onBottom;
    SP<Config::Values::CIntValue> hideBackgroundLayers;
    SP<Config::Values::CIntValue> hideTopLayers;
    SP<Config::Values::CIntValue> hideOverlayLayers;
    SP<Config::Values::CIntValue> drawActiveWorkspace;
    SP<Config::Values::CIntValue> hideRealLayers;
    SP<Config::Values::CIntValue> affectStrut;

    SP<Config::Values::CIntValue> overrideGaps;
    SP<Config::Values::CIntValue> gapsIn;
    SP<Config::Values::CIntValue> gapsOut;

    SP<Config::Values::CIntValue> autoDrag;
    SP<Config::Values::CIntValue> autoScroll;
    SP<Config::Values::CIntValue> exitOnClick;
    SP<Config::Values::CIntValue> switchOnDrop;
    SP<Config::Values::CIntValue> exitOnSwitch;
    SP<Config::Values::CIntValue> showNewWorkspace;
    SP<Config::Values::CIntValue> showEmptyWorkspace;
    SP<Config::Values::CIntValue> showSpecialWorkspace;

    SP<Config::Values::CIntValue> disableGestures;
    SP<Config::Values::CIntValue> reverseSwipe;

    SP<Config::Values::CIntValue> disableBlur;
    SP<Config::Values::CFloatValue> overrideAnimSpeed;
    SP<Config::Values::CFloatValue> dragAlpha;
    SP<Config::Values::CStringValue> exitKey;
};

inline SConfig config = {
    .panelBaseColor = makeShared<Config::Values::CColorValue>("plugin:overview:panelColor", "description", CHyprColor(0, 0, 0, 0).getAsHex()),
    .panelBorderColor = makeShared<Config::Values::CColorValue>("plugin:overview:panelBorderColor", "description", CHyprColor(0, 0, 0, 0).getAsHex()),
    .workspaceActiveBackground = makeShared<Config::Values::CColorValue>("plugin:overview:workspaceActiveBackground", "description", CHyprColor(0, 0, 0, 0.25).getAsHex()),
    .workspaceInactiveBackground = makeShared<Config::Values::CColorValue>("plugin:overview:workspaceInactiveBackground", "description", CHyprColor(0, 0, 0, 0.5).getAsHex()),
    .workspaceActiveBorder = makeShared<Config::Values::CColorValue>("plugin:overview:workspaceActiveBorder", "description", CHyprColor(1, 1, 1, 0.25).getAsHex()),
    .workspaceInactiveBorder = makeShared<Config::Values::CColorValue>("plugin:overview:workspaceInactiveBorder", "description", CHyprColor(1, 1, 1, 0).getAsHex()),

    .panelHeight = makeShared<Config::Values::CIntValue>("plugin:overview:panelHeight", "description", 250),
    .panelBorderWidth = makeShared<Config::Values::CIntValue>("plugin:overview:panelBorderWidth", "description", 2),
    .workspaceMargin = makeShared<Config::Values::CIntValue>("plugin:overview:workspaceMargin", "description", 12),
    .workspaceBorderSize = makeShared<Config::Values::CIntValue>("plugin:overview:workspaceBorderSize", "description", 1),
    .reservedArea = makeShared<Config::Values::CIntValue>("plugin:overview:reservedArea", "description", 0),
    .adaptiveHeight = makeShared<Config::Values::CIntValue>("plugin:overview:adaptiveHeight", "description", 0), // TODO: implement
    .centerAligned = makeShared<Config::Values::CIntValue>("plugin:overview:centerAligned", "description", 1),
    .onBottom = makeShared<Config::Values::CIntValue>("plugin:overview:onBottom", "description", 0), // TODO: implement
    .hideBackgroundLayers = makeShared<Config::Values::CIntValue>("plugin:overview:hideBackgroundLayers", "description", 0),
    .hideTopLayers = makeShared<Config::Values::CIntValue>("plugin:overview:hideTopLayers", "description", 0),
    .hideOverlayLayers = makeShared<Config::Values::CIntValue>("plugin:overview:hideOverlayLayers", "description", 0),
    .drawActiveWorkspace = makeShared<Config::Values::CIntValue>("plugin:overview:drawActiveWorkspace", "description", 1),
    .hideRealLayers = makeShared<Config::Values::CIntValue>("plugin:overview:hideRealLayers", "description", 1),
    .affectStrut = makeShared<Config::Values::CIntValue>("plugin:overview:affectStrut", "description", 1),

    .overrideGaps = makeShared<Config::Values::CIntValue>("plugin:overview:overrideGaps", "description", 1),
    .gapsIn = makeShared<Config::Values::CIntValue>("plugin:overview:gapsIn", "description", 20),
    .gapsOut = makeShared<Config::Values::CIntValue>("plugin:overview:gapsOut", "description", 60),

    .autoDrag = makeShared<Config::Values::CIntValue>("plugin:overview:autoDrag", "description", 1),
    .autoScroll = makeShared<Config::Values::CIntValue>("plugin:overview:autoScroll", "description", 1),
    .exitOnClick = makeShared<Config::Values::CIntValue>("plugin:overview:exitOnClick", "description", 1),
    .switchOnDrop = makeShared<Config::Values::CIntValue>("plugin:overview:switchOnDrop", "description", 0),
    .exitOnSwitch = makeShared<Config::Values::CIntValue>("plugin:overview:exitOnSwitch", "description", 0),
    .showNewWorkspace = makeShared<Config::Values::CIntValue>("plugin:overview:showNewWorkspace", "description", 1),
    .showEmptyWorkspace = makeShared<Config::Values::CIntValue>("plugin:overview:showEmptyWorkspace", "description", 1),
    .showSpecialWorkspace = makeShared<Config::Values::CIntValue>("plugin:overview:showSpecialWorkspace", "description", 0),

    .disableGestures = makeShared<Config::Values::CIntValue>("plugin:overview:disableGestures", "description", 1),
    .reverseSwipe = makeShared<Config::Values::CIntValue>("plugin:overview:reverseSwipe", "description", 0),

    .disableBlur = makeShared<Config::Values::CIntValue>("plugin:overview:disableBlur", "description", 0),
    .overrideAnimSpeed = makeShared<Config::Values::CFloatValue>("plugin:overview:overrideAnimSpeed", "description", 0.0),
    .dragAlpha = makeShared<Config::Values::CFloatValue>("plugin:overview:dragAlpha", "description", 0.2),
    .exitKey = makeShared<Config::Values::CStringValue>("plugin:overview:exitKey", "description", "Escape")
};

namespace HyprConfig {
    inline std::optional<Config::INTEGER> getIntegerSafe(const std::string& name) {
        auto reply = Config::mgr()->getConfigValue(name);
        if (reply.dataptr && reply.type) {
            if (*reply.type == typeid(Config::INTEGER)) {
                return **reinterpret_cast<Config::INTEGER* const*>(reply.dataptr);
            }
        }
        return std::nullopt;
    }

    inline std::optional<Config::FLOAT> getFloatSafe(const std::string& name) {
        auto reply = Config::mgr()->getConfigValue(name);
        if (reply.dataptr && reply.type) {
            if (*reply.type == typeid(Config::FLOAT)) {
                return **reinterpret_cast<Config::FLOAT* const*>(reply.dataptr);
            }
        }
        return std::nullopt;
    }

    inline std::optional<Config::INTEGER> getWorkspaceSwipeDistance() {
        return getIntegerSafe("gestures:workspace_swipe_distance");
    }
    inline std::optional<Config::INTEGER> getWorkspaceSwipeMinSpeedToForce() {
        return getIntegerSafe("gestures:workspace_swipe_min_speed_to_force");
    }
    inline std::optional<Config::FLOAT> getWorkspaceSwipeCancelRatio() {
        return getFloatSafe("gestures:workspace_swipe_cancel_ratio");
    }
    inline std::optional<Config::INTEGER> getHyprsplitNumWorkspaces() {
        return getIntegerSafe("plugin:hyprsplit:num_workspaces");
    }
    inline std::optional<Config::INTEGER> getSplitMonitorWorkspacesCount() {
        return getIntegerSafe("plugin:split-monitor-workspaces:count");
    }
}

namespace Dispatchers {
    SDispatchResult dispatchToggleOverview(std::string arg);
    SDispatchResult dispatchOpenOverview(std::string arg);
    SDispatchResult dispatchCloseOverview(std::string arg);
}

extern int numWorkspaces;
