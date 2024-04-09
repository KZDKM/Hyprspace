# Hyprspace

A plugin for Hyprland that implements a workspace overview feature similar to that of KDE Plasma and macOS, aims to provide a mouse-friendly way of workspace and window management.

## Configuration
### Dispatcher
- Use `overview:toggle` dispatcher to toggle workspace overview on current monitor
### Style
- TBA
### Animation
- The panel use the same curve as the `windows` animation configuration for a slide-in animation

## Goals (Sorted by priority)
- [ ] Base features
    - [x] Overview interface
        - [x] Workspace minimap
        - [x] Workspace display
        - [ ] Workspace name
    - [ ] Mouse controls
        - [ ] Moving window between workspaces
        - [ ] Creating new workspaces
    - [ ] Configurability
- [ ] QoL features
    - [x] Animation support
    - [ ] aarch64 support (CFunctionHook reimpl.)
    - [ ] Gesture support
- [ ] Plugin compatibility
    - [ ] hyprsplit