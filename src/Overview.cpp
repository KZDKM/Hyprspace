#include "Overview.hpp"
#include "Globals.hpp"

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;
    // FIXME: add null check
    curYOffset.create(g_pConfigManager->getAnimationPropertyConfig("windows"), AVARDAMAGE_ENTIRE);
    workspaceScrollOffset.create(g_pConfigManager->getAnimationPropertyConfig("windows"), AVARDAMAGE_ENTIRE);
    curYOffset.setValueAndWarp(Config::panelHeight);
    workspaceScrollOffset.setValueAndWarp(0);
}

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
        ls->fadingOut = false;
    }
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        ls->startAnimation(false);
        ls->readyToDelete = false;
        ls->fadingOut = false;
    }

    active = true;
    curYOffset = 0;
    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        ls->startAnimation(true);
    }
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        ls->startAnimation(true);
    }
    active = false;
    curYOffset = Config::panelHeight * owner->scale;
    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    g_pCompositor->scheduleFrameForMonitor(owner);
}

bool CHyprspaceWidget::isActive() {
    return active;
}
