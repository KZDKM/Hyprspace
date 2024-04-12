# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma and macOS, aims to provide a mouse-friendly way of workspace and window management.

> Only works with the latest git version currently!



https://github.com/KZDKM/Hyprspace/assets/41317840/ed1a585a-30d5-4a79-a6da-8cc0713828f9



## Installation

- Make sure to use `hyprland-git` (versions newer than this commit: https://github.com/hyprwm/Hyprland/commit/ef23ef60c5641c5903f9cf40571ead7ad6aba1b9)

### Manual

To build, have hyprland headers installed and under the repo directory do:
```
make all
```
Then use `hyprctl plugin load` followed by the absolute path to the `.so` file to load

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
    hyprland ={
      # Update for releavant commit, this is just bleeding edge as of 2024/04/11
      url = github:hyprwm/Hyprland/ac0f3411c18497a39498b756b711e092512de9e0;
      inputs.nixpkgs.follows = "nixpkgs";
    };
    Hyprspace = {
      url = github:KZDKM/Hyprspace;
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
- `plugin:overview:workspaceBorderSize`
- `plugin:overview:centerAligned` whether if workspaces should be aligned at the center (KDE / macOS style) or at the left (Windows style)
- `plugin:overview:hideBackgroundLayers` do not draw background layers in overview
- `plugin:overview:drawActiveWorkspace` draw the active workspace in overview as-is

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

## Roadmap
- [x] Overview interface
    - [x] Workspace minimap
    - [x] Workspace display
    - [ ] Workspace labels (workspace ID / name)
- [x] Mouse controls
    - [x] Moving window between workspaces
    - [x] Creating new workspaces
    - [ ] Dragging windows between workspace views
- [x] Configurability
  - [x] Styling
    - [x] Panel background
    - [x] Workspace background & border
    - [ ] Panel on Bottom
    - [ ] Vertical layout (on left / right)
    - [ ] Panel top padding (reserved for bar / notch)
    - [ ] Unique styling for special workspaces
  - [ ] Behavior
    - [x] Autodrag windows
    - [x] Autoscroll workspaces
    - [x] Responsive workspace switching
    - [ ] Responsive exiting 
      - [x] Exit on click / switch
      - [ ] Exit with escape key
    - [ ] Blacklisting workspaces
      - [x] Show / hide new workspace and empty workspaces
      - [ ] Show / hide special workspace (#11)
    - [ ] Blacklisting layers from hiding
- [x] Animation support
- [x] Multi-monitor support
- [x] Monitor scaling support
- [ ] aarch64 support
- [ ] Touchpad & gesture support
  - [ ] Workspace swipe (#9)
  - [ ] Scrolling through workspace panel (untested)
  - [ ] Swipe to open

## Plugin Compatibility
- [x] [hyprsplit](https://github.com/shezdy/hyprsplit) (tested)
