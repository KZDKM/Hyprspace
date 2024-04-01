#pragma once
#include <hyprland/src/Compositor.hpp>

class CHyprspaceWidget {

    bool isActive = false;

    uint64_t ownerID;
    CBox widgetBox;
    CFunctionHook* renderWorkspaceHook;

public:

    CHyprspaceWidget(uint64_t);
    ~CHyprspaceWidget();

    CMonitor* getOwner();

    void show();
    void hide();
    void draw();

};