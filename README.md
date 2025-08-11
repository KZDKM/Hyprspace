# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma, GNOME and macOS, aimed to provide a efficient way of workspace and window management.

> [!NOTE]
> This plugin is still maintained, by combined efforts of me and all the awesome contributors. However, I do not have as much time that I could spend on this plugin as before, and Hyprland is a rapidly changing codebase. Therefore, at this time, I could not guarantee that new issues could be resolved promptly. I appreciate your acknowledgement and support for this project!
> 
> P.S.: I could recommend giving [niri](https://github.com/YaLTeR/niri) a try for anyone who is considering an alternative to Hyprland. It is a scrolling window manager that offers a great built-in overview feature that allows dragging windows in-between workspaces just like this plugin does. It also has better workspace management as it cleans up empty workspaces like GNOME does.

https://github.com/KZDKM/Hyprspace/assets/41317840/ed1a585a-30d5-4a79-a6da-8cc0713828f9

### [Jump to installation](#installation)

## Plugin Compatibility
- [x] [hyprsplit](https://github.com/shezdy/hyprsplit) (tested, explicit support)
- [x] [split-monitor-workspaces](https://github.com/Duckonaut/split-monitor-workspaces) (tested, explicit support)
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
- Use `overview:toggle` dispatcher to toggle workspace overview on current monitor
- Use `overview:close` to close the overview on current monitor if opened
- Use `overview:open` to open the overview on current monitor if closed
- Adding the `all` argument to these dispatchers would toggle / open / close overview on all monitors
### Styling
#### Colors
- `plugin:overview:panelColor`
- `plugin:overview:panelBorderColor`
- `plugin:overview:workspaceActiveBackground`
- `plugin:overview:workspaceInactiveBackground`
- `plugin:overview:workspaceActiveBorder`
- `plugin:overview:workspaceInactiveBorder`
- `plugin:overview:dragAlpha` overrides the alpha of window when dragged in overview (0 - 1, 0 = transparent, 1 = opaque)
- `plugin:overview:disableBlur`
#### Layout
- `plugin:overview:panelHeight`
- `plugin:overview:panelBorderWidth`
- `plugin:overview:onBottom` whether if panel should be on bottom instead of top
- `plugin:overview:workspaceMargin` spacing of workspaces with eachother and the edge of the panel
- `plugin:overview:reservedArea` padding on top of the panel, for Macbook camera notch
- `plugin:overview:workspaceBorderSize`
- `plugin:overview:centerAligned` whether if workspaces should be aligned at the center (KDE / macOS style) or at the left (Windows style)
- `plugin:overview:hideBackgroundLayers` do not draw background and bottom layers in overview
- `plugin:overview:hideTopLayers` do not draw top layers in overview
- `plugin:overview:hideOverlayLayers` do not draw overlay layers in overview
- `plugin:overview:hideRealLayers` whether to hide layers in actual workspace
- `plugin:overview:drawActiveWorkspace` draw the active workspace in overview as-is
- `plugin:overview:overrideGaps` whether if overview should override the layout gaps in the current workspace using the following values
- `plugin:overview:gapsIn`
- `plugin:overview:gapsOut`
- `plugin:overview:affectStrut` whether the panel should push window aside, disabling this option also disables `overrideGaps`

### Animation
- The panel uses the `windows` curve for a slide-in animation
- Use `plugin:overview:overrideAnimSpeed` to override the animation speed

### Behaviors
- `plugin:overview:autoDrag` mouse click always drags window when overview is open
- `plugin:overview:autoScroll` mouse scroll on active workspace area always switch workspace
- `plugin:overview:exitOnClick` mouse click without dragging exits overview
- `plugin:overview:switchOnDrop` switch to the workspace when a window is droppped into it
- `plugin:overview:exitOnSwitch` overview exits when overview is switched by clicking on workspace view or by `switchOnDrop`
- `plugin:overview:showNewWorkspace` add a new empty workspace at the end of workspaces view
- `plugin:overview:showEmptyWorkspace` show empty workspaces that are inbetween non-empty workspaces
- `plugin:overview:showSpecialWorkspace` defaults to false
- `plugin:overview:disableGestures`
- `plugin:overview:reverseSwipe` reverses the direction of swipe gesture, for macOS peeps?
- `plugin:overview:exitKey` key used to exit overview mode (default: Escape). Leave empty to disable keyboard exit.
- Touchpad gesture behavior follows Hyprland workspace swipe behavior
  - `gestures:workspace_swipe_fingers`
  - `gestures:workspace_swipe_cancel_ratio`
  - `gestures:workspace_swipe_min_speed_to_force`
