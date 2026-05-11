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
<<<<<<< HEAD
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/event/EventBus.hpp>

// Hyprland v0.54+: cancellable input uses Event::SCallbackInfo (not legacy CEvent*).
using SCallbackInfo = Event::SCallbackInfo;

// Must match Hyprutils::Signal::CSignalT::RefArg (hyprutils/signal/Signal.hpp).
template <typename T>
using HyprSignalRefArg = std::conditional_t<std::is_trivially_copyable_v<T>, T, const T&>;

// Unpack Hyprutils::CSignalT::emit() tuple — first event arg is often stored by value (trivial types).
template <typename EventType, typename Signal>
CHyprSignalListener listenCancellable(Signal& signal, std::function<void(const EventType&, SCallbackInfo&)> handler) {
=======
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprutils/signal/Signal.hpp>
#include <functional>
#include <tuple>

// Helper to register a cancellable event listener that properly unpacks
// std::tuple<const EventType&, SCallbackInfo&> from the signal's void* args.
template <typename EventType, typename Signal>
CHyprSignalListener listenCancellable(Signal& signal, std::function<void(const EventType&, Event::SCallbackInfo&)> handler) {
>>>>>>> upstream/main
    struct Hack : Hyprutils::Signal::CSignalBase {
        using CSignalBase::registerListenerInternal;
    };
    return reinterpret_cast<Hack&>(signal).registerListenerInternal([handler](void* args) {
<<<<<<< HEAD
        using Tuple = std::tuple<HyprSignalRefArg<EventType>, HyprSignalRefArg<Event::SCallbackInfo&>>;
        auto* tup = static_cast<Tuple*>(args);
=======
        auto* tup = static_cast<std::tuple<const EventType&, Event::SCallbackInfo&>*>(args);
>>>>>>> upstream/main
        handler(std::get<0>(*tup), std::get<1>(*tup));
    });
}

inline HANDLE pHandle = NULL;

<<<<<<< HEAD
typedef void (*tRenderWindow)(void*, PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, Render::eRenderPassMode, bool, bool);
=======
typedef SDispatchResult (*tMouseKeybind)(std::string);
extern void* pMouseKeybind;

typedef void (*tRenderWindow)(void*, PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, eRenderPassMode, bool, bool);
>>>>>>> upstream/main
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, PHLLS, PHLMONITOR, const Time::steady_tp&, bool, bool);
extern void* pRenderLayer;
namespace Config {
    extern CHyprColor panelBaseColor;
    extern CHyprColor panelBorderColor;
    extern CHyprColor workspaceActiveBackground;
    extern CHyprColor workspaceInactiveBackground;
    extern CHyprColor workspaceActiveBorder;
    extern CHyprColor workspaceInactiveBorder;

    extern int panelHeight;
    extern int panelBorderWidth;
    extern int workspaceMargin;
    extern int reservedArea;
    extern int workspaceBorderSize;
    extern bool adaptiveHeight; // TODO: implement
    extern bool centerAligned;
    extern bool onBottom; // TODO: implement
    extern bool hideBackgroundLayers;
    extern bool hideTopLayers;
    extern bool hideOverlayLayers;
    extern bool drawActiveWorkspace;
    extern bool hideRealLayers;
    extern bool affectStrut;

    extern bool overrideGaps;
    extern int gapsIn;
    extern int gapsOut;

    extern bool autoDrag;
    extern bool autoScroll;
    extern bool exitOnClick;
    extern bool switchOnDrop;
    extern bool exitOnSwitch;
    extern bool showNewWorkspace;
    extern bool showEmptyWorkspace;
    extern bool showSpecialWorkspace;

    extern bool disableGestures;
    extern bool reverseSwipe;

    extern bool disableBlur;
    extern float overrideAnimSpeed;
    extern float dragAlpha;
}

extern int numWorkspaces;
