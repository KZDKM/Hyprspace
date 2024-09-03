#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>

inline HANDLE pHandle = NULL;

typedef void (*tMouseKeybind)(std::string);
extern void* pMouseKeybind;

typedef void (*tRenderWindow)(void*, PHLWINDOW, CMonitor*, timespec*, bool, eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, Hyprutils::Memory::CWeakPointer<CLayerSurface>, CMonitor*, timespec*, bool);
extern void* pRenderLayer;

namespace Config {
    extern CColor panelBaseColor;
    extern CColor panelBorderColor;
    extern CColor workspaceActiveBackground;
    extern CColor workspaceInactiveBackground;
    extern CColor workspaceActiveBorder;
    extern CColor workspaceInactiveBorder;

    extern int panelHeight;
    extern int panelBorderWidth;
    extern int workspaceMargin;
    extern int reservedArea;
    extern int workspaceBorderSize;
    extern bool disableBlur;
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

    extern float overrideAnimSpeed;
    extern float dragAlpha;
}

extern int hyprsplitNumWorkspaces;
