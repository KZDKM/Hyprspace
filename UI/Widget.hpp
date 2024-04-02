#pragma once
#include <hyprland/src/Compositor.hpp>

class CHyprspaceWidget {

    bool active = false;

    uint64_t ownerID;
    timespec startTime;
    bool hasStarted = false;
    CBox widgetBox;

    CAnimatedVariable<float> curYOffset; // for slide-in animation

public:

    CHyprspaceWidget(uint64_t);
    ~CHyprspaceWidget();

    CMonitor* getOwner();

    void show();
    void hide();
    void draw(timespec* time);

    bool isActive();

};