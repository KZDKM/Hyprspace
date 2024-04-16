#include "Overview.hpp"
#include "Globals.hpp"

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;

    curAnimationConfig = *g_pConfigManager->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues;
    curAnimationConfig.pValues = &curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    curYOffset.create(&curAnimationConfig, AVARDAMAGE_ENTIRE);
    workspaceScrollOffset.create(&curAnimationConfig, AVARDAMAGE_ENTIRE);
    curYOffset.setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset.setValueAndWarp(0);
}

// TODO: implement deconstructor and delete widget on monitor unplug
CHyprspaceWidget::~CHyprspaceWidget() {}

CMonitor* CHyprspaceWidget::getOwner() {
    return g_pCompositor->getMonitorFromID(ownerID);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;

    if (prevFullscreen.empty()) {
        // unfullscreen all windows
        for (auto& ws : g_pCompositor->m_vWorkspaces) {
            if (ws->m_iMonitorID == ownerID) {
                const auto w = g_pCompositor->getFullscreenWindowOnWorkspace(ws->m_iID);
                if (w != nullptr && ws->m_efFullscreenMode != FULLSCREEN_INVALID) {
                    // we use the getWindowFromHandle function to prevent dangling pointers
                    prevFullscreen.emplace_back(std::make_tuple((uint32_t)(((uint64_t)w) & 0xFFFFFFFF), ws->m_efFullscreenMode));
                    g_pCompositor->setWindowFullscreen(w, false);
                }
            }
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    if (oLayerAlpha.empty()) {
        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.get(), ls->alpha.goal()));
            ls->alpha = 0.f;
            ls->fadingOut = true;
        }
        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.get(), ls->alpha.goal()));
            ls->alpha = 0.f;
            ls->fadingOut = true;
        }
    }

    active = true;

    // swiping panel offset should be handled at updateSwipe
    if (!swiping) {
        curYOffset = 0;
        curSwipeOffset = (Config::panelHeight + Config::reservedArea) * owner->scale;
    }

    //g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (!ls->readyToDelete && ls->mapped && ls->fadingOut) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls.get();});
            if (oAlpha != oLayerAlpha.end()) {
                ls->fadingOut = false;
                ls->alpha = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        if (!ls->readyToDelete && ls->mapped && ls->fadingOut) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls.get();});
            if (oAlpha != oLayerAlpha.end()) {
                ls->fadingOut = false;
                ls->alpha = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    oLayerAlpha.clear();

    // restore fullscreen state
    for (auto& fs : prevFullscreen) {
        const auto w = g_pCompositor->getWindowFromHandle(std::get<0>(fs));
        const auto oFullscreenMode = std::get<1>(fs);
        g_pCompositor->setWindowFullscreen(w, true, oFullscreenMode);
    }
    prevFullscreen.clear();

    active = false;

    if (!swiping) {
        curYOffset = (Config::panelHeight + Config::reservedArea) * owner->scale;
        curSwipeOffset = -10.;
    }
    //g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::updateConfig() {
    curAnimationConfig = *g_pConfigManager->getAnimationPropertyConfig("windows");

    curAnimation = *curAnimationConfig.pValues;
    curAnimationConfig.pValues = &curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    curYOffset.create(&curAnimationConfig, AVARDAMAGE_ENTIRE);
    workspaceScrollOffset.create(&curAnimationConfig, AVARDAMAGE_ENTIRE);
}

bool CHyprspaceWidget::isActive() {
    return active;
}
