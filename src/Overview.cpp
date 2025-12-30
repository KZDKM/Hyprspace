#include "Overview.hpp"
#include "Globals.hpp"

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;

    curAnimationConfig = *g_pConfigManager->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    g_pAnimationManager->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset->setValueAndWarp(0);
}

// TODO: implement deconstructor and delete widget on monitor unplug
CHyprspaceWidget::~CHyprspaceWidget() {}

PHLMONITOR CHyprspaceWidget::getOwner() {
    return g_pCompositor->getMonitorFromID(ownerID);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;

    if (prevFullscreen.empty()) {
        // unfullscreen all windows
        for (auto& ws : g_pCompositor->getWorkspaces()) {
            if (ws->m_monitor->m_id == ownerID) {
                const auto w = ws->getFullscreenWindow();
                if (w != nullptr && ws->m_fullscreenMode != FSMODE_NONE) {
                    // use fakefullscreenstate to preserve client's internal state
                    // fixes youtube fullscreen not restoring properly
                    if (ws->m_fullscreenMode == FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = true;
                    // we use the getWindowFromHandle function to prevent dangling pointers
                    prevFullscreen.emplace_back(std::make_tuple((uint32_t)(((uint64_t)w.get()) & 0xFFFFFFFF), ws->m_fullscreenMode));
                    g_pCompositor->setWindowFullscreenState(w, Desktop::View::SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
                }
            }
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    if (oLayerAlpha.empty() && Config::hideRealLayers) {
        for (auto& ls : owner->m_layerSurfaceLayers[2]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->m_alpha->goal()));
            *ls->m_alpha = 0.f;
            ls->m_fadingOut = true;
        }
        for (auto& ls : owner->m_layerSurfaceLayers[3]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->m_alpha->goal()));
            *ls->m_alpha = 0.f;
            ls->m_fadingOut = true;
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
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& ls : owner->m_layerSurfaceLayers[2]) {
        if (!ls->m_readyToDelete && ls->m_mapped && ls->m_fadingOut) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                ls->m_fadingOut = false;
                *ls->m_alpha = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    for (auto& ls : owner->m_layerSurfaceLayers[3]) {
        if (!ls->m_readyToDelete && ls->m_mapped && ls->m_fadingOut) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                ls->m_fadingOut = false;
                *ls->m_alpha = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    oLayerAlpha.clear();

    // restore fullscreen state
    for (auto& fs : prevFullscreen) {
        const auto w = g_pCompositor->getWindowFromHandle(std::get<0>(fs));
        const auto oFullscreenMode = std::get<1>(fs);
        g_pCompositor->setWindowFullscreenState(w, Desktop::View::SFullscreenState(oFullscreenMode)); 
        if (oFullscreenMode == FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = false;
    }
    prevFullscreen.clear();

    active = false;

    // panel offset should be handled by swipe event when swiping
    if (!swiping) {
        *curYOffset = (Config::panelHeight + Config::reservedArea) * owner->m_scale;
        curSwipeOffset = -10.;
    }

    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::updateConfig() {
    curAnimationConfig = *g_pConfigManager->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    g_pAnimationManager->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset->setValueAndWarp(0);
}

bool CHyprspaceWidget::isActive() {
    return active;
}
