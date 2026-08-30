#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/MonitorState.hpp>

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
        // NOTE: workspace-level fullscreen tracking (m_fullscreenMode/getFullscreenWindow on CWorkspace)
        // was removed and centralized into Fullscreen::controller(); query per-workspace/per-window instead.
        for (auto& ws : State::workspaceState()->workspaces()) {
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID) {
                const auto w = Fullscreen::controller()->getFullscreenWindow(ws.lock());
                if (w != nullptr && Fullscreen::controller()->hasFullscreen(ws.lock())) {
                    const auto oMode = Fullscreen::controller()->getFullscreenModes(w).internal;
                    // use fakefullscreenstate to preserve client's internal state
                    // fixes youtube fullscreen not restoring properly
                    if (oMode == Fullscreen::FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = true;
                    prevFullscreen.emplace_back(std::make_tuple(PHLWINDOWREF(w), oMode));
                    Fullscreen::controller()->setFullscreenMode(w, Fullscreen::FSMODE_NONE, Fullscreen::FSMODE_NONE);
                }
            }
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    if (oLayerAlpha.empty() && Config::hideRealLayers) {
        // NOTE: CLayerSurface::m_alpha is now private; go through the public alpha() accessor.
        // NOTE: CLayerSurface no longer exposes a settable m_fadingOut flag (fade lifecycle moved to
        // Desktop::fadingOutState()/IFadeout); we just drive the alpha directly here.
        for (auto& ls : owner->m_layerSurfaceLayers[2]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha().goal()));
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 0.f;
        }
        for (auto& ls : owner->m_layerSurfaceLayers[3]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha().goal()));
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 0.f;
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
    // NOTE: m_readyToDelete/m_fadingOut no longer exist on CLayerSurface; m_mapped plus the
    // oLayerAlpha membership check is enough to only touch layers we actually hid above.
    for (auto& ls : owner->m_layerSurfaceLayers[2]) {
        if (ls->m_mapped) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    for (auto& ls : owner->m_layerSurfaceLayers[3]) {
        if (ls->m_mapped) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
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
