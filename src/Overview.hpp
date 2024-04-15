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

    std::vector<std::tuple<uint32_t, eFullscreenMode>> prevFullscreen;

    std::chrono::system_clock::time_point lastPressedTime = std::chrono::high_resolution_clock::now();

    bool swiping = false;
    bool activeBeforeSwipe = false;
    double avgSwipeSpeed = 0.;
    int swipePoints = 0;
    double curSwipeOffset = 10.;

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
    void updateLayout();

    bool buttonEvent(bool);
    bool axisEvent(double);

    bool isSwiping();

    bool beginSwipe(wlr_pointer_swipe_begin_event*);
    bool updateSwipe(wlr_pointer_swipe_update_event*);
    bool endSwipe(wlr_pointer_swipe_end_event*);

};