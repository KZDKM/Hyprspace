#include "Overview.hpp"
#include "Globals.hpp"

bool CHyprspaceWidget::buttonEvent(bool pressed) {
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

    int targetWorkspaceID = SPECIAL_WORKSPACE_START - 1;

    // find which workspace the mouse hovers over
    for (auto& w : workspaceBoxes) {
        auto wi = std::get<0>(w);
        auto wb = std::get<1>(w);
        if (wb.containsPoint(g_pInputManager->getMouseCoordsInternal() * getOwner()->scale)) {
            targetWorkspaceID = wi;
            break;
        }
    }

    auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);

    // create new workspace
    if (!targetWorkspace && targetWorkspaceID >= SPECIAL_WORKSPACE_START) {
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
        //g_pHyprRenderer->arrangeLayersForMonitor(getOwner()->ID);
        updateLayout();
    }
    // click workspace to change to workspace and exit overview
    else if (targetWorkspace && !pressed) {
        if (targetWorkspace->m_bIsSpecialWorkspace)
            getOwner()->activeSpecialWorkspaceID() == targetWorkspaceID ? getOwner()->setSpecialWorkspace(nullptr) : getOwner()->setSpecialWorkspace(targetWorkspaceID);
        else {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_iMonitorID)->changeWorkspace(targetWorkspace->m_iID);
        }
        if (Config::exitOnSwitch && active) hide();
    }
    // click elsewhere to exit overview
    else if (Config::exitOnClick && !targetWorkspace.get() && active && couldExit && !pressed) hide();

    return Return;
}

bool CHyprspaceWidget::axisEvent(double delta) {

    CBox widgetBox = {getOwner()->vecPosition.x, getOwner()->vecPosition.y - curYOffset.value(), getOwner()->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * getOwner()->scale};

    if (widgetBox.containsPoint(g_pInputManager->getMouseCoordsInternal() * getOwner()->scale)) {
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

bool CHyprspaceWidget::isSwiping() {
    return swiping;
}

bool CHyprspaceWidget::beginSwipe(wlr_pointer_swipe_begin_event* e) {
    swiping = true;
    activeBeforeSwipe = active;
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}

bool CHyprspaceWidget::updateSwipe(wlr_pointer_swipe_update_event* e) {
    if (!e) return false;
    int fingers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_fingers")->getValue());
    int distance = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_distance")->getValue());
    if (abs(e->dx) / abs(e->dy) < 1) {
        if (swiping && e->fingers == fingers) {

            float currentScaling = g_pCompositor->getMonitorFromCursor()->vecSize.x / distance;

            double scrollDifferential = e->dy * (Config::reverseSwipe ? -1 : 1) * currentScaling;

            curSwipeOffset += scrollDifferential;
            curSwipeOffset = std::clamp<double>(curSwipeOffset, -10, ((Config::panelHeight + Config::reservedArea) * getOwner()->scale));

            avgSwipeSpeed = (avgSwipeSpeed * swipePoints + scrollDifferential) / (swipePoints + 1);

            curYOffset.setValueAndWarp(((Config::panelHeight + Config::reservedArea) * getOwner()->scale) - curSwipeOffset);

            if (curSwipeOffset < 10 && active) hide();
            else if (curSwipeOffset > 10 && !active) show();

            return false;
        }
    }
    else {
        if (e->fingers == fingers && active) {
            CBox widgetBox = {getOwner()->vecPosition.x, getOwner()->vecPosition.y - curYOffset.value(), getOwner()->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * getOwner()->scale};
            if (widgetBox.containsPoint(g_pInputManager->getMouseCoordsInternal() * getOwner()->scale)) {
                workspaceScrollOffset.setValueAndWarp(workspaceScrollOffset.goal() + e->dx * 2);
                return false;
            }
        }
    }
    return true;
}

bool CHyprspaceWidget::endSwipe(wlr_pointer_swipe_end_event* e) {
    swiping = false;
    // force cancel swipe
    if (!e) {
        if (active) hide();
        curSwipeOffset = -10.;
    }
    else {
        int swipeForceSpeed = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_min_speed_to_force")->getValue());
        float cancelRatio = std::any_cast<Hyprlang::FLOAT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_cancel_ratio")->getValue());
        double swipeTravel = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
        if (activeBeforeSwipe) {
            if ((curSwipeOffset < swipeTravel * cancelRatio) || avgSwipeSpeed < -swipeForceSpeed) {
                if (active) hide();
                else {
                    curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                    curSwipeOffset = -10.;
                }
            }
            else {
                // cancel
                if (!active) show();
                else {
                    curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                }
            }
        }
        else {
            if ((curSwipeOffset > swipeTravel * (1.f - cancelRatio)) || avgSwipeSpeed > swipeForceSpeed) {
                if (!active) show();
                else {
                    curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                }
            }
            else {
                // cancel
                if (active) hide();
                else {
                    curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                    curSwipeOffset = -10.;
                }
            }
        }
    }
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}