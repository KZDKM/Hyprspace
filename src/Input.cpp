#include "Overview.hpp"
#include "Globals.hpp"

bool CHyprspaceWidget::mouseEvent(bool pressed) {
    bool Return;

    const auto targetWindow = g_pInputManager->currentlyDraggedWindow;

    // click to exit
    bool couldExit = false;
    if (pressed)
        lastPressedTime = std::chrono::high_resolution_clock::now();
    else
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastPressedTime).count() < 200)
            couldExit = true;

    if (Config::autoDrag) {
        // when overview is active, always drag windows on mouse click
        if (g_pInputManager->currentlyDraggedWindow) {
            g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
            g_pInputManager->currentlyDraggedWindow = nullptr;
            g_pInputManager->dragMode = MBIND_INVALID;
        }
        std::string keybind = (pressed ? "1" : "0") + std::string("movewindow");
        (*(tMouseKeybind)pMouseKeybind)(keybind);
    }
    Return = false;

    int targetWorkspaceID = -1;

    // find which workspace the mouse hovers over
    for (auto& w : workspaceBoxes) {
        auto wi = std::get<0>(w);
        auto wb = std::get<1>(w);
        if (wb.containsPoint(g_pInputManager->getMouseCoordsInternal())) {
            targetWorkspaceID = wi;
            break;
        }
    }

    auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);

    // create new workspace
    if (!targetWorkspace && targetWorkspaceID >= 0) {
        targetWorkspace = g_pCompositor->createNewWorkspace(targetWorkspaceID, getOwner()->ID);
    }

    // release window on workspace to drop it in
    if (targetWindow && targetWorkspace.get() && !pressed) {
        g_pCompositor->moveWindowToWorkspaceSafe(targetWindow, targetWorkspace);
        if (targetWindow->m_bIsFloating) {
            auto targetPos = getOwner()->vecPosition + (getOwner()->vecSize / 2.) - (targetWindow->m_vReportedSize / 2.);
            targetWindow->m_vPosition = targetPos;
            targetWindow->m_vRealPosition = targetPos;
        }
        if (Config::switchOnDrop) {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_iMonitorID)->changeWorkspace(targetWorkspace->m_iID);
            if (Config::exitOnSwitch && active) hide();
        }
        g_pHyprRenderer->arrangeLayersForMonitor(getOwner()->ID);
    }
    // click workspace to change to workspace and exit overview
    else if (targetWorkspace && !pressed) {
        g_pCompositor->getMonitorFromID(targetWorkspace->m_iMonitorID)->changeWorkspace(targetWorkspace->m_iID);
        if (Config::exitOnSwitch && active) hide();
    }
    // click elsewhere to exit overview
    else if (Config::exitOnClick && !targetWorkspace.get() && active && couldExit && !pressed) hide();

    return Return;
}

bool CHyprspaceWidget::axisEvent(double delta) {

    CBox widgetBox = {getOwner()->vecPosition.x, getOwner()->vecPosition.y - curYOffset.value(), getOwner()->vecTransformedSize.x, Config::panelHeight}; //TODO: update size on monitor change

    if (widgetBox.containsPoint(g_pInputManager->getMouseCoordsInternal())) {
        workspaceScrollOffset = workspaceScrollOffset.goal() - delta * 2;
    }
    else {

        if (delta < 0) {
            std::string outName;
            int wsID = getWorkspaceIDFromString("r-1", outName);
            if (g_pCompositor->getWorkspaceByID(wsID) == nullptr) g_pCompositor->createNewWorkspace(wsID, ownerID);
            getOwner()->changeWorkspace(wsID);
        }
        else {
            std::string outName;
            int wsID = getWorkspaceIDFromString("r+1", outName);
            if (g_pCompositor->getWorkspaceByID(wsID) == nullptr) g_pCompositor->createNewWorkspace(wsID, ownerID);
            getOwner()->changeWorkspace(wsID);
        }
    }


    return false;
}