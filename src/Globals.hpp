#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include "Overview.hpp"

inline HANDLE pHandle = NULL;

extern std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;

typedef void (*tRenderWorkspaceWindows)(CHyprRenderer*, CMonitor*, PHLWORKSPACE, timespec*);

typedef void (*tArrangeLayersForMonitor)(CHyprRenderer*, const int&);

typedef void (*tRecalculateMonitor)(void*, const int&);

typedef void (*tChangeWorkspace)(CMonitor*, const PHLWORKSPACE&, bool, bool, bool);

typedef SWorkspaceRule (*tGetWorkspaceRuleFor)(CConfigManager*, PHLWORKSPACE);

typedef bool (*tOnMouseEvent)(CKeybindManager*, wlr_pointer_button_event*);
typedef void (*tMouseKeybind)(std::string);
extern void* pMouseKeybind;

typedef void (*tRenderWindow)(void*, CWindow*, CMonitor*, timespec*, bool, eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, SLayerSurface*, CMonitor*, timespec*, bool);
extern void* pRenderLayer;