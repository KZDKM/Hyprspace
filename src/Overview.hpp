#pragma once
#include <hyprland/src/Compositor.hpp>

class CHyprspaceWidget {

    bool active = false;

    uint64_t ownerID;

    // for slide-in animation
    CAnimatedVariable<float> curYOffset;

    // for checking mouse hover for workspace drag and move
    // modified on draw call, accessed on mouse click and release
    std::vector<std::tuple<int, CBox>> workspaceBoxes;

    std::chrono::system_clock::time_point lastPressedTime = std::chrono::high_resolution_clock::now();

    CAnimatedVariable<float> workspaceScrollOffset;

public:

    CHyprspaceWidget(uint64_t);
    ~CHyprspaceWidget();

    CMonitor* getOwner();
    bool isActive();

    void show();
    void hide();

    void draw(timespec*); // call before renderWorkspaceWindows

    void reserveArea(); // call after arrangeLayersForMonitor
    float reserveGaps(); // TODO: implement

    bool mouseEvent(bool);
    bool axisEvent(double);

};