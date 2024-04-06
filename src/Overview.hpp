#pragma once
#include <hyprland/src/Compositor.hpp>

class CHyprspaceWidget {

    bool active = false;

    uint64_t ownerID;
    timespec startTime;
    bool hasStarted = false;

    CBox widgetBox;

    // for slide-in animation
    CAnimatedVariable<float> curYOffset; 

public:

    // for checking mouse hover for workspace drag and move
    // modified on draw call, accessed on mouse click and release
    // TODO: make private and implement getter
    std::vector<std::tuple<CWorkspace*, std::unique_ptr<CBox>>> workspaceBoxes;

    CHyprspaceWidget(uint64_t);
    ~CHyprspaceWidget();

    static inline CHyprspaceWidget* getWidgetForMonitor(CMonitor*);

    CMonitor* getOwner();
    bool isActive();

    void show();
    void hide();
    void draw(timespec*); // call before renderWorkspaceWindows
    float reserveArea(); // call after arrangeLayersForMonitor
    float reserveGaps(); // TODO: implement
};