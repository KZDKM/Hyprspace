#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;

    curAnimationConfig = *Config::animationTree()->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    Animation::mgr()->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset->setValueAndWarp(0);
}

// TODO: implement deconstructor and delete widget on monitor unplug
CHyprspaceWidget::~CHyprspaceWidget() {}

PHLMONITOR CHyprspaceWidget::getOwner() {
    return State::monitorState()->query().id(ownerID).run();
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;

    if (prevFullscreen.empty()) {
        // unfullscreen all windows
        for (auto& ws : State::workspaceState()->workspacesCopy()) {
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID) {
                const auto w = Fullscreen::controller()->getFullscreenWindow(ws);
                if (w != nullptr && Fullscreen::controller()->getFullscreenModes(ws).internal != Fullscreen::FSMODE_NONE) {
                    // use fakefullscreenstate to preserve client's internal state
                    // fixes youtube fullscreen not restoring properly
                    if (Fullscreen::controller()->getFullscreenModes(ws).internal == Fullscreen::FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = true;
                    prevFullscreen.emplace_back(std::make_tuple(PHLWINDOWREF(w), Fullscreen::controller()->getFullscreenModes(ws).internal));
                    Fullscreen::controller()->setFullscreenMode(w, Fullscreen::FSMODE_NONE, Fullscreen::FSMODE_NONE);
                }
            }
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    if (oLayerAlpha.empty() && Config::hideRealLayers) {
        for (auto& ls : owner->m_layerSurfaceLayers[2]) {
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha()[Desktop::View::LS_ALPHA_FADE]->goal()));
            ls->alpha()[Desktop::View::LS_ALPHA_FADE]->setValueAndWarp(0.f);
        }
        for (auto& ls : owner->m_layerSurfaceLayers[3]) {
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha()[Desktop::View::LS_ALPHA_FADE]->goal()));
            ls->alpha()[Desktop::View::LS_ALPHA_FADE]->setValueAndWarp(0.f);
        }
    }

    active = true;

    // panel offset should be handled by swipe event when swiping
    if (!swiping) {
        *curYOffset = 0;
        curSwipeOffset = (Config::panelHeight + Config::reservedArea) * owner->m_scale;
    }

    updateLayout();
    g_pHyprRenderer->damageMonitor(owner);
    owner->scheduleFrame();
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& tuple : oLayerAlpha) {
        const auto ls = std::get<0>(tuple);
        if (ls && ls->m_mapped)
            ls->alpha()[Desktop::View::LS_ALPHA_FADE]->setValueAndWarp(std::get<1>(tuple));
    }
    oLayerAlpha.clear();

    // restore fullscreen state
    for (auto& fs : prevFullscreen) {
        const auto w = std::get<0>(fs).lock();
        if (!w) continue;
        const auto oFullscreenMode = std::get<1>(fs);
        Fullscreen::controller()->setFullscreenMode(w, oFullscreenMode, oFullscreenMode);
        if (oFullscreenMode == Fullscreen::FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = false;
    }
    prevFullscreen.clear();

    active = false;

    // panel offset should be handled by swipe event when swiping
    if (!swiping) {
        *curYOffset = (Config::panelHeight + Config::reservedArea) * owner->m_scale;
        curSwipeOffset = -10.;
    }

    updateLayout();
    owner->scheduleFrame();
}

void CHyprspaceWidget::updateConfig() {
    curAnimationConfig = *Config::animationTree()->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    Animation::mgr()->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset->setValueAndWarp(0);
}

bool CHyprspaceWidget::isActive() {
    return active;
}
