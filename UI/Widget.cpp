#include "Widget.hpp"
#include "../global.hpp"

constexpr int g_panelHeight = 150;
constexpr int g_workspaceMargin = 12;
constexpr bool g_workspacesCenterAlign = true;

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;
    curYOffset.create(g_pConfigManager->getAnimationPropertyConfig("windowsIn"), AVARDAMAGE_ENTIRE);
    curYOffset.setValueAndWarp(g_panelHeight);
}

CMonitor* CHyprspaceWidget::getOwner() {
    return g_pCompositor->getMonitorFromID(ownerID);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;


    // hide top and overlay layers
    // TODO: ensure input is disabled for hidden layers
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
    curYOffset = g_panelHeight;
    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    g_pCompositor->scheduleFrameForMonitor(owner);
}

bool CHyprspaceWidget::isActive() {
    return active;
}

void renderWindowStub(CWindow* pWindow, CMonitor* pMonitor, CWorkspace* pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    int oWorkspaceID = pWindow->m_iWorkspaceID;
    bool oFullscreen = pWindow->m_bIsFullscreen;
    Vector2D oPosition = pWindow->m_vRealPosition.value();
    Vector2D oSize = pWindow->m_vRealSize.value();
    float oRounding = pWindow->rounding();
    bool oUseNearestNeighbor = pWindow->m_sAdditionalConfigData.nearestNeighbor.toUnderlying();
    Vector2D oRenderOffset = pWorkspaceOverride->m_vRenderOffset.value(); // this is zeroed out so that the miniature windows isnt animated on workspace switch

    pWindow->m_iWorkspaceID = pWorkspaceOverride->m_iID;
    pWindow->m_bIsFullscreen = false;
    pWindow->m_vRealPosition.setValue(rectOverride.pos());
    pWindow->m_vRealSize.setValue(rectOverride.size());
    pWindow->m_sAdditionalConfigData.rounding = oRounding * (rectOverride.w / oSize.x);
    pWindow->m_sAdditionalConfigData.nearestNeighbor = true; // FIX: we need proper downscaling so that windows arent shown as pixelated mess
    pWorkspaceOverride->m_vRenderOffset.setValue({0, 0});

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, false, RENDER_PASS_MAIN, false, false);

    // restore values for normal window render
    pWindow->m_iWorkspaceID = oWorkspaceID;
    pWindow->m_bIsFullscreen = oFullscreen;
    pWindow->m_vRealPosition.setValue(oPosition);
    pWindow->m_vRealSize.setValue(oSize);
    pWindow->m_sAdditionalConfigData.rounding = oRounding;
    pWindow->m_sAdditionalConfigData.nearestNeighbor = oUseNearestNeighbor;
    pWorkspaceOverride->m_vRenderOffset.setValue(oRenderOffset);
}

void renderLayerStub(SLayerSurface* pLayer, CMonitor* pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    Vector2D oPosition = pLayer->realPosition.value();
    Vector2D oSize = pLayer->realSize.value();
    float oAlpha = pLayer->alpha.value(); // set to 1 temporarily to show hidden top layer

    pLayer->realPosition.setValue(rectOverride.pos());
    pLayer->realSize.setValue(rectOverride.size());
    pLayer->alpha.setValue(1);

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->realPosition.setValue(oPosition);
    pLayer->realSize.setValue(oSize);
    pLayer->alpha.setValue(oAlpha);
}

float CHyprspaceWidget::reserveArea() {
    return g_panelHeight - curYOffset.goal();
}


void CHyprspaceWidget::draw(timespec* time) {

    if (!active && !curYOffset.isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    if (curYOffset.isBeingAnimated()) {
        g_pCompositor->scheduleFrameForMonitor(owner);
    }

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = true; // need to set to true to keep blur framebuffer when no window is present

    // TODO: set clipbox to the current monitor so that the slide in animation dont fuck up other monitors
    widgetBox = CBox({owner->vecPosition.x, owner->vecPosition.y - curYOffset.value()}, {owner->vecTransformedSize.x, g_panelHeight}); //TODO: update size on monitor change
    g_pHyprRenderer->damageBox(&widgetBox);
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, CColor(0, 0, 0, 0)); // need blurfbshouldrender = true

    std::vector<CWorkspace*> workspaces;
    int activeWorkspaceID = 0;
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_iMonitorID == ownerID) {
            workspaces.push_back(ws.get());
            if (ws->m_iID == owner->activeWorkspace)
                activeWorkspaceID = ws->m_iID;
        }
    }

    // render workspace boxes center aligned
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = (g_panelHeight - 2 * g_workspaceMargin) / owner->vecTransformedSize.y; // scale box with panel height
    double workspaceBoxW = owner->vecTransformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->vecTransformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + g_workspaceMargin * (wsCount - 1);
    double curWorkspaceRectOffsetX = g_workspacesCenterAlign ? owner->vecPosition.x + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : owner->vecPosition.x + g_workspaceMargin;
    double curWorkspaceRectOffsetY = owner->vecPosition.y + g_workspaceMargin - curYOffset.value();

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;

    for (auto ws : workspaces) {
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};
        if (ws->m_iID == activeWorkspaceID)
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 1)); // cant really round it until I find a proper way to clip windows to a rounded rect
        else
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 0.5));


        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
            g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
            renderLayerStub(ls.get(), owner, layerBox, time);
            g_pHyprOpenGL->m_RenderData.clipBox = CBox();
        }
        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
            g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
            renderLayerStub(ls.get(), owner, layerBox, time);
            g_pHyprOpenGL->m_RenderData.clipBox = CBox();
        }

        if (owner->activeWorkspace != ws->m_iID) {

            // draw tiled windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_iWorkspaceID == ws->m_iID && !w->m_bIsFloating) {
                    double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                    double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(1, 1, 1, 1), 12);
                    renderWindowStub(w.get(), owner, g_pCompositor->getWorkspaceByID(owner->activeWorkspace), curWindowBox, time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_iWorkspaceID == ws->m_iID && w->m_bIsFloating && w.get() != ws->getLastFocusedWindow()) {
                    double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                    double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(1, 1, 1, 1), 12);
                    renderWindowStub(w.get(), owner, g_pCompositor->getWorkspaceByID(owner->activeWorkspace), curWindowBox, time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_bIsFloating) {
                    CWindow* w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                    double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(1, 1, 1, 1), 12);
                    renderWindowStub(w, owner, g_pCompositor->getWorkspaceByID(owner->activeWorkspace), curWindowBox, time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }


            // this layer is hidden for real workspace on toggle show
            for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.get(), owner, layerBox, time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }

        // no reasons to render overlay layers atm...

        curWorkspaceRectOffsetX += workspaceBoxW + g_workspaceMargin;
    }
}