#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include "UI/Widget.hpp"

inline HANDLE pHandle = NULL;

extern std::vector<CHyprspaceWidget*> g_overviewWidgets;

typedef void (*tRenderWorkspaceWindows)(CHyprRenderer*, CMonitor*, CWorkspace*, timespec*);
extern CFunctionHook* renderWorkspaceWindowsHook;

typedef void (*tArrangeLayersForMonitor)(CHyprRenderer*, const int&);
extern CFunctionHook* arrangeLayersForMonitorHook;

typedef void (*tRecalculateMonitor)(void*, const int&);
extern CFunctionHook* recalculateMonitorHook;

typedef void (*tChangeWorkspace)(CMonitor*, CWorkspace* const, bool, bool, bool);
extern CFunctionHook* changeWorkspaceHook;

typedef std::vector<SWorkspaceRule> (*tGetWorkspaceRulesFor)(CConfigManager*, CWorkspace*);
extern CFunctionHook* getWorkspaceRulesForHook;

typedef void (*tRenderWindow)(void*, CWindow*, CMonitor*, timespec*, bool, eRenderPassMode, bool, bool);
extern void* pRenderWindow;
typedef void (*tRenderLayer)(void*, SLayerSurface*, CMonitor*, timespec*, bool);
extern void* pRenderLayer;