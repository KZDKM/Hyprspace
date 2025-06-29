# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma, GNOME and macOS, aimed to provide a efficient way of workspace and window management.

- Supports Hyprland release `>= 0.39`. All supported release versions will be pinned. New features and fixes will NOT be backported. Please report build issues on any supported release version.

> Also checkout [hyprexpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo) from the official plugin repo that provides a grid style overview!


https://github.com/KZDKM/Hyprspace/assets/41317840/ed1a585a-30d5-4a79-a6da-8cc0713828f9

### [Jump to installation](#installation)

## Plugin Compatibility
- [x] [hyprsplit](https://github.com/shezdy/hyprsplit) (tested, explicit support)
- [x] [hyprexpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo) (tested, minor bugs)
- [x] Any layout plugin (except ones that override Hyprland's workspace management)

## Roadmap
- [x] Overview interface
    - [x] Workspace minimap
    - [x] Workspace display
- [ ] Mouse controls
    - [x] Moving window between workspaces
    - [x] Creating new workspaces
    - [ ] Dragging windows between workspace views
- [ ] Configurability
  - [ ] Styling
    - [x] Panel background
    - [x] Workspace background & border
    - [x] Panel on Bottom
    - [ ] Vertical layout (on left / right)
    - [x] Panel top padding (reserved for bar / notch)
    - [x] Panel border (color / thickness)
    - [ ] Unique styling for special workspaces
  - [x] Behavior
    - [x] Autodrag windows
    - [x] Autoscroll workspaces
    - [x] Responsive workspace switching
    - [x] Responsive exiting 
      - [x] Exit on click / switch
      - [x] Exit with escape key
    - [x] Blacklisting workspaces
      - [x] Show / hide new workspace and empty workspaces
      - [x] Show / hide special workspace (#11)
- [x] Animation support
- [x] Multi-monitor support (tested)
- [x] Monitor scaling support (tested)
- [x] aarch64 support (No function hook used)
- [x] Touchpad & gesture support
  - [x] Workspace swipe (#9)
  - [x] Scrolling through workspace panel
  - [x] Swipe to open

## Installation

### Manual

To build, have hyprland headers installed and under the repo directory do:
```
make all
```
Then use `hyprctl plugin load` followed by the absolute path to the `.so` file to load, you could add this to your `exec-once` to load the plugin on startup

### Hyprpm
```
hyprpm add https://github.com/KZDKM/Hyprspace
hyprpm enable Hyprspace
```

### Nix
Refer to the [Hyprland wiki](https://wiki.hyprland.org/Nix/Hyprland-on-Home-Manager/#plugins) on plugins, but your flake might look like this:
```nix
{
  inputs = {
    # Hyprland is **such** eye candy
    hyprland = {
      type = "git";
      url = "https://github.com/hyprwm/Hyprland";
      submodules = true;
      inputs.nixpkgs.follows = "nixpkgs";
    };
    Hyprspace = {
      url = "github:KZDKM/Hyprspace";

      # Hyprspace uses latest Hyprland. We declare this to keep them in sync.
      inputs.hyprland.follows = "hyprland";
    };
  };

... # your normal setup with hyprland

  wayland.windowManager.hyprland.plugins = [
    # ... whatever
    inputs.Hyprspace.packages.${pkgs.system}.Hyprspace
  ];
}
```

## Usage

### Opening Overview
- Bind the `overview:toggle` or perform a workspace swipe vertically would open / close the panel.
### Interaction
- Window management:
    - Click on workspace to change to it
    - Click on a window to drag it
    - Drag a window into a workspace would move the window to the workspace
- Exiting
    - Click without dragging the window exits the overview
    - Pressing ESC exits the overview
- Navigating
    - When there are many workspaces open, scroll / swipe on the panel to pan through opened workspaces

## Configuration

### Dispatchers

| Dispatcher | Description |
|------------|-------------|
| `overview:toggle` | Toggle workspace overview on current monitor |
| `overview:close` | Close the overview on current monitor if opened |
| `overview:open` | Open the overview on current monitor if closed |

Adding the `all` argument to these dispatchers will toggle/open/close overview on all monitors.

### Variables

Set any of these in your hyprland configuration under `plugin:overview`.

#### Colors

| Variable | Description | Type | Default |
|----------|-------------|------|---------|
| `panelColor` | Background color of the overview panel | color | `rgba(0, 0, 0, 0.8)` |
| `panelBorderColor` | Border color of the overview panel | color | `rgba(255, 255, 255, 0.2)` |
| `workspaceActiveBackground` | Background color of the active workspace | color | `rgba(255, 255, 255, 0.1)` |
| `workspaceInactiveBackground` | Background color of inactive workspaces | color | `rgba(0, 0, 0, 0.1)` |
| `workspaceActiveBorder` | Border color of the active workspace | color | `rgba(255, 255, 255, 0.8)` |
| `workspaceInactiveBorder` | Border color of inactive workspaces | color | `rgba(255, 255, 255, 0.3)` |
| `dragAlpha` | Alpha of window when dragged in overview | float | `0.5` |

#### Layout

| Variable | Description | Type | Default |
|----------|-------------|------|---------|
| `panelHeight` | Height of the overview panel | int | `200` |
| `panelBorderWidth` | Border width of the overview panel | int | `2` |
| `onBottom` | Position panel at bottom instead of top | bool | `false` |
| `workspaceMargin` | Spacing between workspaces and panel edges | int | `10` |
| `reservedArea` | Top padding for bars/notches | int | `0` |
| `workspaceBorderSize` | Border width of workspace views | int | `2` |
| `centerAligned` | Center-align workspaces (KDE/macOS style) vs left-align (Windows style) | bool | `true` |
| `overrideGaps` | Override layout gaps in current workspace | bool | `false` |
| `gapsIn` | Inner gaps when `overrideGaps` is enabled | int | `5` |
| `gapsOut` | Outer gaps when `overrideGaps` is enabled | int | `10` |
| `affectStrut` | Panel pushes windows aside (disabling this also disables `overrideGaps`) | bool | `true` |

#### Visibility

| Variable | Description | Type | Default |
|----------|-------------|------|---------|
| `hideBackgroundLayers` | Hide background and bottom layers in overview | bool | `false` |
| `hideTopLayers` | Hide top layers in overview | bool | `false` |
| `hideOverlayLayers` | Hide overlay layers in overview | bool | `false` |
| `hideRealLayers` | Hide layers in actual workspace | bool | `false` |
| `drawActiveWorkspace` | Draw the active workspace in overview as-is | bool | `true` |
| `disableBlur` | Disable blur effects | bool | `false` |

#### Animation

| Variable | Description | Type | Default |
|----------|-------------|------|---------|
| `overrideAnimSpeed` | Override animation speed | float | `1.0` |

*Note: The panel uses the `windows` curve for slide-in animation.*

#### Behavior

| Variable | Description | Type | Default |
|----------|-------------|------|---------|
| `autoDrag` | Mouse click always drags window when overview is open | bool | `false` |
| `autoScroll` | Mouse scroll on active workspace area switches workspace | bool | `true` |
| `exitOnClick` | Mouse click without dragging exits overview | bool | `true` |
| `switchOnDrop` | Switch to workspace when window is dropped into it | bool | `true` |
| `exitOnSwitch` | Exit overview when switching workspace by clicking or dropping | bool | `false` |
| `showNewWorkspace` | Show empty workspace at end for creating new ones | bool | `true` |
| `showEmptyWorkspace` | Show empty workspaces between non-empty ones | bool | `false` |
| `showSpecialWorkspace` | Show special workspaces in overview | bool | `false` |
| `disableGestures` | Disable touchpad gestures | bool | `false` |
| `reverseSwipe` | Reverse swipe gesture direction (macOS style) | bool | `false` |
| `exitKey` | Key to exit overview mode (empty to disable) | string | `Escape` |

*Note: Touchpad gesture behavior follows Hyprland workspace swipe settings (`gestures:workspace_swipe_*`).*


