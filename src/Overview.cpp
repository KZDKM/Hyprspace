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

    // unfullscreen all windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iMonitorID == ownerID) {
            g_pCompositor->setWindowFullscreen(w.get(), false);
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        ls->startAnimation(false);
        ls->readyToDelete = false;
    }
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        ls->startAnimation(false);
        ls->readyToDelete = false;
    }

    active = true;
    curYOffset = 0;
    //g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        ls->fadingOut = false;
        ls->startAnimation(true);
    }
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        ls->fadingOut = false;
        ls->startAnimation(true);
    }
    active = false;
    curYOffset = Config::panelHeight * owner->scale;
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
