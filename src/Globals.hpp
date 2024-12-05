#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>

inline HANDLE pHandle = NULL;

typedef SDispatchResult (*tMouseKeybind)(std::string);
extern void* pMouseKeybind;

typedef void (*tRenderWindow)(void*, PHLWINDOW, PHLMONITOR, timespec*, bool, eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, Hyprutils::Memory::CWeakPointer<CLayerSurface>, PHLMONITOR, timespec*, bool);
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

extern int hyprsplitNumWorkspaces;
