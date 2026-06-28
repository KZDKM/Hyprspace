#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

#include "Overview.hpp"
#include "Globals.hpp"

namespace {
    int findTargetWorkspaceID(const std::vector<std::tuple<int, CBox>>& workspaceBoxes, Vector2D coords, bool allowFallback) {
        if (workspaceBoxes.empty())
            return SPECIAL_WORKSPACE_START - 1;

        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();
        double bestDistance = std::numeric_limits<double>::max();
        int bestWorkspaceID = SPECIAL_WORKSPACE_START - 1;

        for (const auto& w : workspaceBoxes) {
            const auto workspaceID = std::get<0>(w);
            const auto& box = std::get<1>(w);

            if (box.containsPoint(coords))
                return workspaceID;

            minX = std::min(minX, box.x);
            minY = std::min(minY, box.y);
            maxX = std::max(maxX, box.x + box.w);
            maxY = std::max(maxY, box.y + box.h);

            const auto center = box.middle();
            const auto dx = center.x - coords.x;
            const auto dy = center.y - coords.y;
            const auto distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                bestWorkspaceID = workspaceID;
            }
        }

        if (!allowFallback)
            return SPECIAL_WORKSPACE_START - 1;

        constexpr double padding = 96.0;
        const bool nearWorkspaceStrip = coords.x >= minX - padding && coords.x <= maxX + padding && coords.y >= minY - padding && coords.y <= maxY + padding;
        if (!nearWorkspaceStrip || bestWorkspaceID < SPECIAL_WORKSPACE_START)
            return SPECIAL_WORKSPACE_START - 1;

        return bestWorkspaceID;
    }
}

bool CHyprspaceWidget::buttonEvent(bool pressed, Vector2D coords) {
    bool Return;

    const auto dragTarget = g_layoutManager->dragController()->target();
    const auto targetWindow = dragTarget ? dragTarget->window() : nullptr;

    // this is for click to exit, we set a timeout for button release
    bool couldExit = false;
    if (pressed)
        lastPressedTime = std::chrono::high_resolution_clock::now();
    else
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastPressedTime).count() < 200)
            couldExit = true;

    const auto targetWorkspaceID = findTargetWorkspaceID(workspaceBoxes, coords, targetWindow != nullptr && !pressed);
    auto targetWorkspace = g_pCompositor->getWorkspaceByID(targetWorkspaceID);

    // create new workspace
    if (targetWorkspace == nullptr && targetWorkspaceID >= SPECIAL_WORKSPACE_START) {
        targetWorkspace = g_pCompositor->createNewWorkspace(targetWorkspaceID, getOwner()->m_id);
    }

    // if the cursor is hovering over workspace, clicking should switch workspace instead of starting window drag
    if (Config::autoDrag && (targetWorkspace == nullptr || !pressed)) {
        if (g_layoutManager->dragController()->target())
            g_layoutManager->endDragTarget();

        if (pressed) {
            const auto PWINDOW = g_pCompositor->vectorToWindowUnified(coords, Desktop::View::WINDOW_ONLY, nullptr);
            if (PWINDOW) {
                const auto LT = PWINDOW->layoutTarget();
                if (LT)
                    g_layoutManager->beginDragTarget(LT, MBIND_MOVE);
            }
        }
    }
    Return = false;

    // release window on workspace to drop it in
    if (targetWindow && targetWorkspace != nullptr && !pressed) {
        g_pCompositor->moveWindowToWorkspaceSafe(targetWindow, targetWorkspace);
        if (targetWindow->m_isFloating) {
            auto targetPos = getOwner()->m_position + (getOwner()->m_size / 2.) - (targetWindow->m_reportedSize / 2.);
            targetWindow->m_position = targetPos;
            *targetWindow->m_realPosition = targetPos;
        }
        if (Config::switchOnDrop) {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_monitor->m_id)->changeWorkspace(targetWorkspace->m_id);
            if (Config::exitOnSwitch && active) hide();
        }
        updateLayout();
    }
    // click workspace to change to workspace and exit overview
    else if (targetWorkspace && !pressed) {
        if (targetWorkspace->m_isSpecialWorkspace)
            getOwner()->activeSpecialWorkspaceID() == targetWorkspaceID ? getOwner()->setSpecialWorkspace(nullptr) : getOwner()->setSpecialWorkspace(targetWorkspaceID);
        else {
            g_pCompositor->getMonitorFromID(targetWorkspace->m_monitor->m_id)->changeWorkspace(targetWorkspace->m_id);
        }
        if (Config::exitOnSwitch && active) hide();
    }
    // click elsewhere to exit overview
    else if (Config::exitOnClick && targetWorkspace == nullptr && active && couldExit && !pressed) {
        hide();
    }

    return Return;
}

bool CHyprspaceWidget::axisEvent(double delta, wl_pointer_axis axis, Vector2D coords) {

    const auto owner = getOwner();
    CBox widgetBox = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};
    if (Config::onBottom) widgetBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale) + curYOffset->value(), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    // scroll through panel if cursor is on it
    if (widgetBox.containsPoint(coords * getOwner()->m_scale)) {
        // only horizontal scroll pans the panel; ignore vertical scroll here
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            *workspaceScrollOffset = workspaceScrollOffset->goal() - delta * 2;
    }
    // otherwise, scroll to switch active workspace (vertical scroll only)
    else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (delta < 0) {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r-1");
            if (g_pCompositor->getWorkspaceByID(wsIDName.id) == nullptr) {
                auto newWorkspace = g_pCompositor->createNewWorkspace(wsIDName.id, ownerID);
                (void)newWorkspace;
            }
            getOwner()->changeWorkspace(wsIDName.id);
        }
        else {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r+1");
            if (g_pCompositor->getWorkspaceByID(wsIDName.id) == nullptr) {
                auto newWorkspace = g_pCompositor->createNewWorkspace(wsIDName.id, ownerID);
                (void)newWorkspace;
            }
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
    constexpr int fingers = 3;
    int distance = std::any_cast<Hyprlang::INT>(HyprlandAPI::getConfigValue(pHandle, "gestures:workspace_swipe_distance")->getValue());

    // restrict swipe to a axis with the most significant movement to prevent misinput
    if (abs(e.delta.x) / abs(e.delta.y) < 1) {
        if (swiping && e.fingers == (uint32_t)fingers) {

            float currentScaling = g_pCompositor->getMonitorFromCursor()->m_size.x / distance;

            double scrollDifferential = e.delta.y * (Config::reverseSwipe ? -1 : 1) * (Config::onBottom ? -1 : 1) * currentScaling;

            curSwipeOffset += scrollDifferential;
            curSwipeOffset = std::clamp<double>(curSwipeOffset, -10, ((Config::panelHeight + Config::reservedArea) * getOwner()->m_scale));

            avgSwipeSpeed = (avgSwipeSpeed * swipePoints + scrollDifferential) / (swipePoints + 1);

            curYOffset->setValueAndWarp(((Config::panelHeight + Config::reservedArea) * getOwner()->m_scale) - curSwipeOffset);

            if (curSwipeOffset < 10 && active) hide();
            else if (curSwipeOffset > 10 && !active) show();

            return false;
        }
    }
    else {
        // scroll through panel
        if (e.fingers == (uint32_t)fingers && active) {
            const auto owner = getOwner();
            CBox widgetBox = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};
            if (Config::onBottom) widgetBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale) + curYOffset->value(), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};
            if (widgetBox.containsPoint(g_pInputManager->getMouseCoordsInternal() * getOwner()->m_scale)) {
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
        double swipeTravel = (Config::panelHeight + Config::reservedArea) * getOwner()->m_scale;
        if (activeBeforeSwipe) {
            if ((curSwipeOffset < swipeTravel * cancelRatio) || avgSwipeSpeed < -swipeForceSpeed) {
                if (active) hide();
                else {
                    *curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->m_scale;
                    curSwipeOffset = -10.;
                }
            }
            else {
                // cancel
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->m_scale;
                }
            }
        }
        else {
            if ((curSwipeOffset > swipeTravel * (1.f - cancelRatio)) || avgSwipeSpeed > swipeForceSpeed) {
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->m_scale;
                }
            }
            else {
                // cancel
                if (active) hide();
                else {
                    *curYOffset = (Config::panelHeight + Config::reservedArea) * getOwner()->m_scale;
                    curSwipeOffset = -10.;
                }
            }
        }
    }
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}
