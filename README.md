# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma and macOS, aims to provide a mouse-friendly way of workspace and window management.

> Only works with the latest git version currently!

## Configuration
### Dispatchers
- Use `overview:toggle` dispatcher to toggle workspace overview on current monitor
- Use `overview:close` to close the overview on current monitor if opened
- Use `overview:open` to open the overview on current monitor if closed
- Adding the `all` argument to `overview:close` and `overview:open` would open / close overview on all monitors
### Styling
#### Colors
- `plugin:overview:panelColor`
- `plugin:overview:workspaceActiveBackground`
- `plugin:overview:workspaceInactiveBackground`
- `plugin:overview:workspaceActiveBorder`
- `plugin:overview:workspaceInactiveBorder`
- `plugin:overview:dragAlpha` overrides the alpha of window when dragged in overview (0 - 1, 0 = transparent, 1 = opaque)
#### Layout
- `plugin:overview:panelHeight`
- `plugin:overview:workspaceMargin` spacing of workspaces with eachother and the edge of the panel
- `plugin:overview:centerAligned` whether if workspaces should be aligned at the center (KDE / macOS style) or at the left (Windows style)
- `plugin:overview:overrideGaps` whether if the following tiling gap values should be applied when workspace is open
- `plugin:overview:gapsIn`
- `plugin:overview:gapsOut`
- `plugin:overview:hideBackgroundLayers` do not draw background layers in overview
- `plugin:overview:drawActiveWorkspace` draw the active workspace in overview as-is (looks wonky so defaults to false)

### Animation
- The panel uses the `windows` curve for a slide-in animation

### Behaviors
- `plugin:overview:autoDrag` mouse click always drags window when overview is open
- `plugin:overview:autoScroll` mouse scroll on active workspace area always switch workspace
- `plugin:overview:exitOnClick` mouse click without dragging exits overview
- `plugin:overview:switchOnDrop` switch to the workspace when a window is droppped into it
- `plugin:overview:exitOnSwitch` overview exits when overview is switched by clicking on workspace view or by `switchOnDrop`
- `plugin:overview:showNewWorkspace` add a new empty workspace at the end of workspaces view
- `plugin:overview:showEmptyWorkspace` show empty workspaces that are inbetween non-empty workspaces

## Features (Sorted by priority)
- [ ] Overview interface
    - [x] Workspace minimap
    - [x] Workspace display
    - [ ] Workspace name
- [x] Mouse controls
    - [x] Moving window between workspaces
    - [x] Creating new workspaces
- [x] Configurability
- [x] Animation support
- [x] Multi-monitor support
- [ ] aarch64 support (CFunctionHook reimplementation)
- [ ] Gesture support

## Installation

### Manual

To build, have hyprland headers installed and under the repo directory do:
```
meson setup ./builddir ./
cd builddir
meson compile
```
Then use `hyprctl plugin load` followed by the absolute path to the `.so` file in builddir to load

### Hyprpm
- TBA

## Plugin Compatibility
- [x] [hyprsplit](https://github.com/shezdy/hyprsplit)