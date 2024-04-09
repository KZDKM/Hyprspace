# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma and macOS, aims to provide a mouse-friendly way of workspace and window management.

## Configuration
### Dispatchers
- Use `overview:toggle` dispatcher to toggle workspace overview on current monitor
- Use `overview:close` to close the overview if opened
### Styling
- TBA
### Animation
- The panel use the same curve as the `windows` animation configuration for a slide-in animation
### Behaviors
- TBA

## Features (Sorted by priority)
- [ ] Base features
    - [ ] Overview interface
        - [x] Workspace minimap
        - [x] Workspace display
        - [ ] Workspace name
    - [x] Mouse controls
        - [x] Moving window between workspaces
        - [x] Creating new workspaces
    - [ ] Configurability
- [ ] QoL features
    - [x] Animation support
    - [x] Multi-monitor support
    - [ ] aarch64 support (CFunctionHook reimpl.)
    - [ ] Gesture support