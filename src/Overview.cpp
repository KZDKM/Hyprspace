#include "Overview.hpp"
#include "Globals.hpp"

constexpr int g_panelHeight = 250;
constexpr int g_workspaceMargin = 12;
constexpr bool g_workspacesCenterAlign = true;


CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;
    // FIXME: add null check
    curYOffset.create(g_pConfigManager->getAnimationPropertyConfig("windows"), AVARDAMAGE_ENTIRE);
    curYOffset.setValueAndWarp(g_panelHeight);
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
    curYOffset = g_panelHeight;
    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
    g_pCompositor->scheduleFrameForMonitor(owner);
}

bool CHyprspaceWidget::isActive() {
    return active;
}

void renderWindowStub(CWindow* pWindow, CMonitor* pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    // just kill me
    const auto oWorkspace = pWindow->m_pWorkspace;
    const auto oFullscreen = pWindow->m_bIsFullscreen;
    const auto oPosition = pWindow->m_vPosition;
    const auto oRealPosition = pWindow->m_vRealPosition.value();
    const auto oSize = pWindow->m_vReportedSize;
    const auto oUseNearestNeighbor = pWindow->m_sAdditionalConfigData.nearestNeighbor.toUnderlying();
    const auto oRenderOffset = pWorkspaceOverride->m_vRenderOffset.value();
    const auto oDraggedWindow = g_pInputManager->currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->dragMode;
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;

    // hack
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, (float)(rectOverride.w / oSize.x)});
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
    pWindow->m_pWorkspace = pWorkspaceOverride;
    pWindow->m_bIsFullscreen = false;
    pWindow->m_vPosition = ((pMonitor->vecPosition * (rectOverride.w / oSize.x)) + rectOverride.pos()) / (rectOverride.w / oSize.x);
    pWindow->m_vRealPosition.setValue(((pMonitor->vecPosition * (rectOverride.w / oSize.x)) + rectOverride.pos()) / (rectOverride.w / oSize.x));
    pWindow->m_sAdditionalConfigData.nearestNeighbor = false; // FIX: this wont do, need to scale surface texture down properly so that windows arent shown as pixelated mess
    pWorkspaceOverride->m_vRenderOffset.setValue({0, 0}); // no workspace sliding, bPinned = true also works
    g_pInputManager->currentlyDraggedWindow = pWindow; // override these and force INTERACTIVERESIZEINPROGRESS = true to trick the renderer
    g_pInputManager->dragMode = MBIND_RESIZE;

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, false, RENDER_PASS_MAIN, false, true); // ignoreGeometry needs to be true

    // restore values for normal window render
    pWindow->m_pWorkspace = oWorkspace;
    pWindow->m_bIsFullscreen = oFullscreen;
    pWindow->m_vPosition = oPosition;
    pWindow->m_vRealPosition.setValue(oRealPosition);
    pWindow->m_sAdditionalConfigData.nearestNeighbor = oUseNearestNeighbor;
    pWorkspaceOverride->m_vRenderOffset.setValue(oRenderOffset);
    g_pInputManager->currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->dragMode = oDragMode;
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = oRenderModifEnable;
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
}

void renderLayerStub(SLayerSurface* pLayer, CMonitor* pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    Vector2D oRealPosition = pLayer->realPosition.value();
    Vector2D oSize = pLayer->realSize.value();
    float oAlpha = pLayer->alpha.value(); // set to 1 to show hidden top layer

    pLayer->realPosition.setValue(rectOverride.pos());
    pLayer->realSize.setValue(rectOverride.size());
    pLayer->alpha.setValue(1);

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->realPosition.setValue(oRealPosition);
    pLayer->realSize.setValue(oSize);
    pLayer->alpha.setValue(oAlpha);
}

float CHyprspaceWidget::reserveArea() {
    return g_panelHeight - curYOffset.goal();
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck?
void CHyprspaceWidget::draw(timespec* time) {

    workspaceBoxes.clear();

    if (!active && !curYOffset.isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    if (curYOffset.isBeingAnimated()) {
        g_pCompositor->scheduleFrameForMonitor(owner);
    }

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = true; // true to keep blur framebuffer when no window is present

    // TODO: set clipbox to the current monitor so that the slide in animation dont fuck up other monitors
    widgetBox = {owner->vecPosition.x, owner->vecPosition.y - curYOffset.value(), owner->vecTransformedSize.x, g_panelHeight}; //TODO: update size on monitor change

    g_pHyprRenderer->damageBox(&widgetBox);
    widgetBox.x -= owner->vecPosition.x;
    widgetBox.y -= owner->vecPosition.y;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox({0, 0}, owner->vecTransformedSize);
    // crashes with xray true
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, CColor(0, 0, 0, 0), 0, 1.f, false); // need blurfbshouldrender = true
    g_pHyprOpenGL->m_RenderData.clipBox = CBox();

    std::vector<CWorkspace*> workspaces;
    int activeWorkspaceID = 0;
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_iMonitorID == ownerID) {
            workspaces.push_back(ws.get());
            if (owner->activeWorkspace == ws)
                activeWorkspaceID = ws->m_iID;
        }
    }

    // create new workspace at end if last workspace isnt empty
    /*auto lastWorkspace = std::max_element(workspaces.begin(), workspaces.end(),
        [] (const CWorkspace* w1, const CWorkspace* w2) {
            if (!w1 || !w1) return false;
            else return w1->m_iID < w2->m_iID;
        });

    if ((*lastWorkspace))
        if (g_pCompositor->getWindowsOnWorkspace((*lastWorkspace)->m_iID) != 0) {
            workspaces.push_back(g_pCompositor->createNewWorkspace((*lastWorkspace)->m_iID + 1, ownerID).get());
        }*/


        // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = (g_panelHeight - 2 * g_workspaceMargin) / owner->vecTransformedSize.y; // scale box with panel height
    double workspaceBoxW = owner->vecTransformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->vecTransformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + g_workspaceMargin * (wsCount - 1);
    double curWorkspaceRectOffsetX = g_workspacesCenterAlign ? 0 + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : 0 + g_workspaceMargin;
    double curWorkspaceRectOffsetY = 0 + g_workspaceMargin - curYOffset.value();

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;

    for (auto& ws : workspaces) {
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};
        if (ws->m_iID == activeWorkspaceID)
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 0.5), 0, 1.f, false); // cant really round it until I find a proper way to clip windows to a rounded rect
        else
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 0.25), 0, 1.f, false);


        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
            CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
            layerBox.x += owner->vecPosition.x;
            layerBox.y += owner->vecPosition.y;
            g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
            renderLayerStub(ls.get(), owner, layerBox, time);
            g_pHyprOpenGL->m_RenderData.clipBox = CBox();
        }
        for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
            CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
            layerBox.x += owner->vecPosition.x;
            layerBox.y += owner->vecPosition.y;
            g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
            renderLayerStub(ls.get(), owner, layerBox, time);
            g_pHyprOpenGL->m_RenderData.clipBox = CBox();
        }

        if (owner->activeWorkspace.get() == ws) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            g_pHyprOpenGL->renderRectWithBlur(&miniPanelBox, CColor(0, 0, 0, 0), 0, 1.f, false);
        }

        // draw tiled windows
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w) continue;
            if (w->m_pWorkspace.get() == ws && !w->m_bIsFloating) {
                double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                if (!(wW > 0 && wH > 0)) continue;
                CBox curWindowBox = {wX, wY, wW, wH};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(1, 1, 1, 1), 12);
                renderWindowStub(w.get(), owner, owner->activeWorkspace, curWindowBox, time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }
        // draw floating windows
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w) continue;
            if (w->m_pWorkspace.get() == ws && w->m_bIsFloating && w.get() != ws->getLastFocusedWindow()) {
                double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                if (!(wW > 0 && wH > 0)) continue;
                CBox curWindowBox = {wX, wY, wW, wH};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(1, 1, 1, 1), 12);
                renderWindowStub(w.get(), owner, owner->activeWorkspace, curWindowBox, time);
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
                renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }


        if (owner->activeWorkspace.get() != ws) {
            // this layer is hidden for real workspace when panel is displayed
            for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                layerBox.x += owner->vecPosition.x;
                layerBox.y += owner->vecPosition.y;
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.get(), owner, layerBox, time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }

        // no reasons to render overlay layers atm...

        curWorkspaceBox.x += owner->vecPosition.x;
        curWorkspaceBox.y += owner->vecPosition.y;
        workspaceBoxes.emplace_back(std::make_tuple(ws->m_iID, curWorkspaceBox));

        curWorkspaceRectOffsetX += workspaceBoxW + g_workspaceMargin;
    }
}