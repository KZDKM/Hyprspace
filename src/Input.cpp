#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include "Overview.hpp"
#include "Globals.hpp"

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

    std::optional<SOverviewWorkspace> hovered;

    // find which workspace the mouse hovers over
    for (auto& w : workspaceBoxes) {
        const auto& tile = std::get<0>(w);
        const auto& box = std::get<1>(w);
        if (box.containsPoint(coords)) {
            hovered = tile;
            break;
        }
    }

    PHLWORKSPACE targetWorkspace = nullptr;
    if (hovered) {
        if (hovered->special)
            targetWorkspace = ensureDefaultSpecialWorkspace(getOwner());
        else
            targetWorkspace = ensureNumberedWorkspace(hovered->numbered, getOwner());
    }

    // if the cursor is hovering over workspace, clicking should switch workspace instead of starting window drag
    if (config.autoDrag->value() && (targetWorkspace == nullptr || !pressed)) {
        if (g_layoutManager->dragController()->target())
            g_layoutManager->endDragTarget();

        if (pressed) {
            const auto PWINDOW = Desktop::viewState()->hitTest().windowAt(coords, Desktop::View::WINDOW_ONLY, nullptr);
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
        Desktop::globalWindowController()->moveWindowToWorkspace(targetWindow, targetWorkspace);
        if (targetWindow->isFloating()) {
            auto targetPos = getOwner()->m_position + (getOwner()->m_size / 2.) - (targetWindow->backend().reportedSize() / 2.);
            targetWindow->move(targetPos);
        }
        if (config.switchOnDrop->value()) {
            if (auto monitor = State::monitorState()->query().id(targetWorkspace->m_monitor->m_id).run())
                monitor->changeWorkspace(targetWorkspace);
            if (config.exitOnSwitch->value() && active) hide();
        }
        updateLayout();
    }
    // click workspace to change to workspace and exit overview
    else if (targetWorkspace && !pressed) {
        auto owner = getOwner();
        if (targetWorkspace->type() == Workspace::eWorkspaceType::SPECIAL) {
            if (owner->m_activeSpecialWorkspace == targetWorkspace)
                owner->setSpecialWorkspace(nullptr);
            else
                owner->setSpecialWorkspace(targetWorkspace);
        }
        else if (auto monitor = State::monitorState()->query().id(targetWorkspace->m_monitor->m_id).run())
            monitor->changeWorkspace(targetWorkspace);
        if (config.exitOnSwitch->value() && active) hide();
    }
    // click elsewhere to exit overview
    else if (config.exitOnClick->value() && targetWorkspace == nullptr && active && couldExit && !pressed) hide();

    return Return;
}

bool CHyprspaceWidget::axisEvent(double delta, wl_pointer_axis axis, Vector2D coords) {

    const auto owner = getOwner();
    CBox widgetBox = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale};
    if (config.onBottom->value()) widgetBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - ((config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale) + curYOffset->value(), owner->m_transformedSize.x, (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale};

    // scroll through panel if cursor is on it
    if (widgetBox.containsPoint(coords * getOwner()->m_scale)) {
        // only horizontal scroll pans the panel; ignore vertical scroll here
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            *workspaceScrollOffset = workspaceScrollOffset->goal() - delta * 2;
    }
    // otherwise, scroll to switch active workspace (vertical scroll only)
    else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (auto next = relativeNumberedWorkspace(getOwner(), delta < 0 ? -1 : 1))
            getOwner()->changeWorkspace(next);
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
    const auto distance = HyprConfig::getWorkspaceSwipeDistance().value();

    // restrict swipe to a axis with the most significant movement to prevent misinput
    if (abs(e.delta.x) / abs(e.delta.y) < 1) {
        if (swiping && e.fingers == (uint32_t)fingers) {

            float currentScaling = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run()->m_size.x / distance;

            double scrollDifferential = e.delta.y * (config.reverseSwipe->value() ? -1 : 1) * (config.onBottom->value() ? -1 : 1) * currentScaling;

            curSwipeOffset += scrollDifferential;
            curSwipeOffset = std::clamp<double>(curSwipeOffset, -10, ((config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale));

            avgSwipeSpeed = (avgSwipeSpeed * swipePoints + scrollDifferential) / (swipePoints + 1);

            curYOffset->setValueAndWarp(((config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale) - curSwipeOffset);

            if (curSwipeOffset < 10 && active) hide();
            else if (curSwipeOffset > 10 && !active) show();

            return false;
        }
    }
    else {
        // scroll through panel
        if (e.fingers == (uint32_t)fingers && active) {
            const auto owner = getOwner();
            CBox widgetBox = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale};
            if (config.onBottom->value()) widgetBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - ((config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale) + curYOffset->value(), owner->m_transformedSize.x, (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale};
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
        const auto swipeForceSpeed = HyprConfig::getWorkspaceSwipeMinSpeedToForce().value();
        const auto cancelRatio = HyprConfig::getWorkspaceSwipeCancelRatio().value();
        double swipeTravel = (config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale;
        if (activeBeforeSwipe) {
            if ((curSwipeOffset < swipeTravel * cancelRatio) || avgSwipeSpeed < -swipeForceSpeed) {
                if (active) hide();
                else {
                    *curYOffset = (config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale;
                    curSwipeOffset = -10.;
                }
            }
            else {
                // cancel
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale;
                }
            }
        }
        else {
            if ((curSwipeOffset > swipeTravel * (1.f - cancelRatio)) || avgSwipeSpeed > swipeForceSpeed) {
                if (!active) show();
                else {
                    *curYOffset = 0;
                    curSwipeOffset = (config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale;
                }
            }
            else {
                // cancel
                if (active) hide();
                else {
                    *curYOffset = (config.panelHeight->value() + config.reservedArea->value()) * getOwner()->m_scale;
                    curSwipeOffset = -10.;
                }
            }
        }
    }
    avgSwipeSpeed = 0;
    swipePoints = 0;
    return false;
}
