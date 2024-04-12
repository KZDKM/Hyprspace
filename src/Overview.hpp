#pragma once
#include <hyprland/src/Compositor.hpp>

class CHyprspaceWidget {

    bool active = false;

    uint64_t ownerID;

    SAnimationPropertyConfig curAnimationConfig;
    SAnimationPropertyConfig curAnimation;

    // for checking mouse hover for workspace drag and move
    // modified on draw call, accessed on mouse click and release
    std::vector<std::tuple<int, CBox>> workspaceBoxes;

    std::chrono::system_clock::time_point lastPressedTime = std::chrono::high_resolution_clock::now();

    CAnimatedVariable<float> workspaceScrollOffset;

public:

    // for slide-in animation
    CAnimatedVariable<float> curYOffset;

    CHyprspaceWidget(uint64_t);
    ~CHyprspaceWidget();

    CMonitor* getOwner();
    bool isActive();

    void show();
    void hide();

    void updateConfig();

    void draw(); // call before renderWorkspaceWindows

    // reserves area on owner monitor
    void reserveArea();
    float reserveGaps();

    bool mouseEvent(bool);
    bool axisEvent(double);

};