#include "Widget.hpp"
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>


CHyprspaceWidget::CHyprspaceWidget(uint64_t ID) {
    ownerID = ID;
    auto pMonitor = getOwner();
    widgetBox = CBox(pMonitor->vecPosition, {pMonitor->vecPixelSize.x, 300});
}

CMonitor* CHyprspaceWidget::getOwner() {
    return g_pCompositor->getMonitorFromID(ownerID);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;


    // hide top layers
    for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        ls.get()->startAnimation(false);
    }

    //owner->addDamage(&widgetBox);

}

void CHyprspaceWidget::draw() {
    auto owner = getOwner();
    if (!owner) return;

    widgetBox = CBox(owner->vecPosition, {owner->vecPixelSize.x, 300}); //TODO: update size on monitor change
    g_pHyprRenderer->damageBox(&widgetBox);
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, CColor(0, 0, 0, 0)); // we need blurfbshouldrender = true for blur
}