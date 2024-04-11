#include "Overview.hpp"
#include "Globals.hpp"

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
    const auto oFloating = pWindow->m_bIsFloating;

    // hack
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, (float)(rectOverride.w / oSize.x)});
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
    pWindow->m_pWorkspace = pWorkspaceOverride;
    pWindow->m_bIsFullscreen = false;
    pWindow->m_vPosition = ((pMonitor->vecPosition * (rectOverride.w / oSize.x)) + rectOverride.pos()) / (rectOverride.w / oSize.x);
    pWindow->m_vRealPosition.setValue(((pMonitor->vecPosition * (rectOverride.w / oSize.x)) + rectOverride.pos()) / (rectOverride.w / oSize.x));
    pWindow->m_sAdditionalConfigData.nearestNeighbor = false; // FIX: this wont do, need to scale surface texture down properly so that windows arent shown as pixelated mess
    pWindow->m_bIsFloating = false; // weird shit happened so hack fix
    pWorkspaceOverride->m_vRenderOffset.setValue({0, 0}); // no workspace sliding, bPinned = true also works
    g_pInputManager->currentlyDraggedWindow = pWindow; // override these and force INTERACTIVERESIZEINPROGRESS = true to trick the renderer
    g_pInputManager->dragMode = MBIND_RESIZE;


    g_useMipmapping = true;
    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, false, RENDER_PASS_MAIN, false, true); // ignoreGeometry needs to be true
    g_useMipmapping = false;

    // restore values for normal window render
    pWindow->m_pWorkspace = oWorkspace;
    pWindow->m_bIsFullscreen = oFullscreen;
    pWindow->m_vPosition = oPosition;
    pWindow->m_vRealPosition.setValue(oRealPosition);
    pWindow->m_sAdditionalConfigData.nearestNeighbor = oUseNearestNeighbor;
    pWindow->m_bIsFloating = oFloating;
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

    g_useMipmapping = true;
    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);
    g_useMipmapping = false;

    pLayer->realPosition.setValue(oRealPosition);
    pLayer->realSize.setValue(oSize);
    pLayer->alpha.setValue(oAlpha);
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
    CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y - curYOffset.value(), owner->vecTransformedSize.x, Config::panelHeight}; //TODO: update size on monitor change

    g_pHyprRenderer->damageBox(&widgetBox);
    widgetBox.x -= owner->vecPosition.x;
    widgetBox.y -= owner->vecPosition.y;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox({0, 0}, owner->vecTransformedSize);
    // crashes with xray true
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, Config::panelBaseColor, 0, 1.f, false); // need blurfbshouldrender = true
    g_pHyprOpenGL->m_RenderData.clipBox = CBox();

    std::vector<int> workspaces;
    int lowestID = INFINITY;
    int highestID = 1;
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (!ws) continue;
        if (ws->m_iMonitorID == ownerID) {
            workspaces.push_back(ws->m_iID);
            if (highestID < ws->m_iID) highestID = ws->m_iID;
            if (lowestID > ws->m_iID) lowestID = ws->m_iID;
        }
    }

    if (Config::showEmptyWorkspace) {
        int wsIDStart = 1;
        int wsIDEnd = highestID;
        // hyprsplit compatibility
        // fixes the problem that empty workspaces which should belong to previous monitors being shown
        if (hyprsplitNumWorkspaces > 0) {
            wsIDStart = std::min<int>(hyprsplitNumWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(hyprsplitNumWorkspaces * ownerID + 1, highestID); // always show the initial workspace for current monitor
        }
        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            const auto pWorkspace = g_pCompositor->getWorkspaceByID(i);
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    if (Config::showNewWorkspace) {
        // get the lowest empty workspce id after the highest id of current workspace
        while (g_pCompositor->getWorkspaceByID(highestID) != nullptr) highestID++;

        // add new empty workspace
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());


    // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = (Config::panelHeight - 2 * Config::workspaceMargin) / owner->vecTransformedSize.y; // scale box with panel height
    double workspaceBoxW = owner->vecTransformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->vecTransformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + Config::workspaceMargin * (wsCount - 1);
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset.value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset.value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = 0 + Config::workspaceMargin - curYOffset.value();
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + Config::workspaceMargin, 0);

    workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset.goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};
        if (ws == owner->activeWorkspace) {
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 0.5), 0, 1.f, false); // cant really round it until I find a proper way to clip windows to a rounded rect
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin;
                continue;
            }
        }
        else
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, CColor(0, 0, 0, 0.25), 0, 1.f, false);

        if (!Config::hideBackgroundLayers) {
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
        }

        if (owner->activeWorkspace == ws) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            g_pHyprOpenGL->renderRectWithBlur(&miniPanelBox, CColor(0, 0, 0, 0), 0, 1.f, false);
        }

        if (ws != nullptr) {

            // draw tiled windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && !w->m_bIsFloating) {
                    double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                    double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
                    renderWindowStub(w.get(), owner, owner->activeWorkspace, curWindowBox, time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && w->m_bIsFloating && ws->getLastFocusedWindow() != w.get()) {
                    double wX = curWorkspaceRectOffsetX + (w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor;
                    double wY = curWorkspaceRectOffsetY + (w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor;
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
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
                    g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
        }

        if (owner->activeWorkspace != ws) {
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
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin;
    }
}
