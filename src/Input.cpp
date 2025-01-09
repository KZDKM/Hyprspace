#include "Overview.hpp"
#include "Globals.hpp"

bool CHyprspaceWidget::buttonEvent(bool pressed, Vector2D coords) {
    bool Return;

    const auto targetWindow = g_pInputManager->currentlyDraggedWindow.lock();

    // this is for click to exit, we set a timeout for button release
    bool couldExit = false;
    if (pressed)
        lastPressedTime = std::chrono::high_resolution_clock::now();
    else
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastPressedTime).count() < 200)
            couldExit = true;

    int targetWorkspaceID = SPECIAL_WORKSPACE_START - 1;

    // find which workspace the mouse hovers over
    for (auto& w : workspaceBoxes) {
        auto wi = std::get<0>(w);
        auto wb = std::get<1>(w);
        if (wb.containsPoint(coords)) {
            targetWorkspaceID = wi;
            break;
        }
    }

    auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);

    // create new workspace
    if (targetWorkspace == nullptr && targetWorkspaceID >= SPECIAL_WORKSPACE_START) {
        targetWorkspace = g_pCompositor->createNewWorkspace(targetWorkspaceID, getOwner()->ID);
    }

    // if the cursor is hovering over workspace, clicking should switch workspace instead of starting window drag
    if (Config::autoDrag && (targetWorkspace == nullptr || !pressed)) {
        // when overview is active, always drag windows on mouse click
        if (const auto curWindow = g_pInputManager->currentlyDraggedWindow.lock()) {
            g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
            g_pInputManager->currentlyDraggedWindow.reset();
            g_pInputManager->dragMode = MBIND_INVALID;
        }
        std::string keybind = (pressed ? "1" : "0") + std::string("movewindow");
        (*(tMouseKeybind)pMouseKeybind)(keybind);
    }
    Return = false;

    // release window on workspace to drop it in
    if (targetWindow && targetWorkspace != nullptr && !pressed) {
        g_pCompositor->moveWindowToWorkspaceSafe(targetWindow, targetWorkspace);
        if (targetWindow->m_bIsFloating) {
            auto targetPos = getOwner()->vecPosition + (getOwner()->vecSize / 2.) - (targetWindow->m_vReportedSize / 2.);
            targetWindow->m_vPosition = targetPos;
            *targetWindow->m_vRealPosition = targetPos;
        }
        if (Config::switchOnDrop) {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_pMonitor->ID)->changeWorkspace(targetWorkspace->m_iID);
            if (Config::exitOnSwitch && active) hide();
        }
        updateLayout();
    }
    // click workspace to change to workspace and exit overview
    else if (targetWorkspace && !pressed) {
        if (targetWorkspace->m_bIsSpecialWorkspace)
            getOwner()->activeSpecialWorkspaceID() == targetWorkspaceID ? getOwner()->setSpecialWorkspace(nullptr) : getOwner()->setSpecialWorkspace(targetWorkspaceID);
        else {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_pMonitor->ID)->changeWorkspace(targetWorkspace->m_iID);
        }
        if (Config::exitOnSwitch && active) hide();
    }
    // click elsewhere to exit overview
    else if (Config::exitOnClick && targetWorkspace == nullptr && active && couldExit && !pressed) hide();

    return Return;
}

bool CHyprspaceWidget::axisEvent(double delta, Vector2D coords) {

    const auto owner = getOwner();
    CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y - curYOffset->value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};
    if (Config::onBottom) widgetBox = {owner->vecPosition.x, owner->vecPosition.y + owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->scale) + curYOffset->value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};

    // scroll through panel if cursor is on it
    if (widgetBox.containsPoint(coords * getOwner()->scale)) {
        *workspaceScrollOffset = workspaceScrollOffset->goal() - delta * 2;
    }
    // otherwise, scroll to switch active workspace
    else {
        if (delta < 0) {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r-1");
            if (g_pCompositor->getWorkspaceByID(wsIDName.id) == nullptr) g_pCompositor->createNewWorkspace(wsIDName.id, ownerID);
            getOwner()->changeWorkspace(wsIDName.id);
        }
        else {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r+1");
            if (g_pCompositor->getWorkspaceByID(wsIDName.id) == nullptr) g_pCompositor->createNewWorkspace(wsIDName.id, ownerID);
            getOwner()->changeWorkspace(wsIDName.id);
        }
    }


    return false;
}

bool CHyprspaceWidget::isSwiping() {
    return swiping;
}

bool CHyprspaceWidget::beginSwipe(IPointer::SSwipeBeginEvent e) {
    swiping = true;
    activeBeforeSwipe = active;
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}

bool CHyprspaceWidget::updateSwipe(IPointer::SSwipeUpdateEvent e) {
    int fingers = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_fingers")->getValue());
    int distance = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_distance")->getValue());

    // restrict swipe to a axis with the most significant movement to prevent misinput
    if (abs(e.delta.x) / abs(e.delta.y) < 1) {
        if (swiping && e.fingers == (uint32_t)fingers) {

            float currentScaling = g_pCompositor->getMonitorFromCursor()->vecSize.x / distance;

            double scrollDifferential = e.delta.y * (Config::reverseSwipe ? -1 : 1) * (Config::onBottom ? -1 : 1) * currentScaling;

            curSwipeOffset += scrollDifferential;
            curSwipeOffset = std::clamp<double>(curSwipeOffset, -10, ((Config::panelHeight + Config::reservedArea) * getOwner()->scale));

            avgSwipeSpeed = (avgSwipeSpeed * swipePoints + scrollDifferential) / (swipePoints + 1);

            curYOffset->setValueAndWarp(((Config::panelHeight + Config::reservedArea) * getOwner()->scale) - curSwipeOffset);

            if (curSwipeOffset < 10 && active) hide();
            else if (curSwipeOffset > 10 && !active) show();

            return false;
        }
    }
    else {
        // scroll through panel
        if (e.fingers == (uint32_t)fingers && active) {
            const auto owner = getOwner();
            CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y - curYOffset->value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};
            if (Config::onBottom) widgetBox = {owner->vecPosition.x, owner->vecPosition.y + owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->scale) + curYOffset->value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};
            if (widgetBox.containsPoint(g_pInputManager->getMouseCoordsInternal() * getOwner()->scale)) {
                workspaceScrollOffset->setValueAndWarp(workspaceScrollOffset->goal() + e.delta.x * 2);
                return false;
            }
        }
    }
    // otherwise, do not cancel the event and perform workspace swipe normally
    return true;
}

// janky asf
bool CHyprspaceWidget::endSwipe(IPointer::SSwipeEndEvent e) {
    swiping = false;
    // force cancel swipe
    if (e.cancelled) {
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
                    *curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                    curSwipeOffset = -10.;
                }
            }
            else {
                // cancel
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                }
            }
        }
        else {
            if ((curSwipeOffset > swipeTravel * (1.f - cancelRatio)) || avgSwipeSpeed > swipeForceSpeed) {
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                }
            }
            else {
                // cancel
                if (active) hide();
                else {
                    *curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->scale;
                    curSwipeOffset = -10.;
                }
            }
        }
    }
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}
