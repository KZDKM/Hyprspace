#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <numeric>

inline HANDLE pHandle = NULL;

typedef void (*tMouseKeybind)(std::string);
extern void* pMouseKeybind;

typedef void (*tRenderWindow)(void*, CWindow*, CMonitor*, timespec*, bool, eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, SLayerSurface*, CMonitor*, timespec*, bool);
extern void* pRenderLayer;

namespace Config {
    extern CColor panelBaseColor;
    extern CColor workspaceActiveBackground;
    extern CColor workspaceInactiveBackground;
    extern CColor workspaceActiveBorder;
    extern CColor workspaceInactiveBorder;

    extern int panelHeight;
    extern int workspaceMargin;
    extern int reservedArea;
    extern int workspaceBorderSize;
    extern bool adaptiveHeight; // TODO: implement
    extern bool centerAligned;
    extern bool onTop; // TODO: implement
    extern bool hideBackgroundLayers;
    extern bool hideTopLayers;
    extern bool hideOverlayLayers;
    extern bool drawActiveWorkspace;

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

    extern float overrideAnimSpeed;
    extern float dragAlpha;
}

extern int hyprsplitNumWorkspaces;