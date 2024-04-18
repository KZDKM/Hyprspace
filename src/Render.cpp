#include "Overview.hpp"
#include "Globals.hpp"

void renderWindowStub(CWindow* pWindow, CMonitor* pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    const auto oWorkspace = pWindow->m_pWorkspace;
    const auto oFullscreen = pWindow->m_bIsFullscreen;
    const auto oRealPosition = pWindow->m_vRealPosition.value();
    const auto oSize = pWindow->m_vRealSize.value();
    const auto oUseNearestNeighbor = pWindow->m_sAdditionalConfigData.nearestNeighbor.toUnderlying();
    const auto oPinned = pWindow->m_bPinned;
    const auto oDraggedWindow = g_pInputManager->currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->dragMode;
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;
    const auto oFloating = pWindow->m_bIsFloating;
    const auto oSpecialRounding = pWindow->m_sAdditionalConfigData.rounding;

    const float curScaling = rectOverride.w / (oSize.x * pMonitor->scale);

    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, (pMonitor->vecPosition * pMonitor->scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->scale)});
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
    pWindow->m_pWorkspace = pWorkspaceOverride;
    pWindow->m_bIsFullscreen = false; // FIXME: no windows should be in fullscreen when overview is open, reject all fullscreen requests when active
    pWindow->m_sAdditionalConfigData.nearestNeighbor = false; // FIX: this wont do, need to scale surface texture down properly so that windows arent shown as pixelated mess
    pWindow->m_bIsFloating = false; // weird shit happened so hack fix
    pWindow->m_bPinned = true;
    pWindow->m_sAdditionalConfigData.rounding = pWindow->rounding() * pMonitor->scale * curScaling;
    g_pInputManager->currentlyDraggedWindow = pWindow; // override these and force INTERACTIVERESIZEINPROGRESS = true to trick the renderer
    g_pInputManager->dragMode = MBIND_RESIZE;

    g_pHyprRenderer->damageWindow(pWindow);

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_MAIN, false, false);

    // restore values for normal window render
    pWindow->m_pWorkspace = oWorkspace;
    pWindow->m_bIsFullscreen = oFullscreen;
    pWindow->m_sAdditionalConfigData.nearestNeighbor = oUseNearestNeighbor;
    pWindow->m_bIsFloating = oFloating;
    pWindow->m_bPinned = oPinned;
    pWindow->m_sAdditionalConfigData.rounding = oSpecialRounding;
    g_pInputManager->currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->dragMode = oDragMode;
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = oRenderModifEnable;
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
}

void renderLayerStub(SLayerSurface* pLayer, CMonitor* pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    if (!pLayer->mapped || pLayer->readyToDelete || !pLayer->layerSurface) return;

    Vector2D oRealPosition = pLayer->realPosition.value();
    Vector2D oSize = pLayer->realSize.value();
    float oAlpha = pLayer->alpha.value(); // set to 1 to show hidden top layer
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;
    const auto oFadingOut = pLayer->fadingOut;

    const float curScaling = rectOverride.w / (oSize.x);

    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, pMonitor->vecPosition + (rectOverride.pos() / curScaling) - oRealPosition});
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
    pLayer->alpha.setValue(1);
    pLayer->fadingOut = false;

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->fadingOut = oFadingOut;
    pLayer->alpha.setValue(oAlpha);
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = oRenderModifEnable;
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck?
void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset.isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    g_pCompositor->scheduleFrameForMonitor(owner);

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = true;
    //g_pHyprOpenGL->markBlurDirtyForMonitor(owner);
    //g_pHyprOpenGL->preRender(owner);

    CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y - curYOffset.value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale}; //TODO: update size on monitor change

    if (Config::onBottom) widgetBox = {owner->vecPosition.x, owner->vecPosition.y + owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->scale) + curYOffset.value(), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};

    g_pHyprRenderer->damageBox(&widgetBox);
    widgetBox.x -= owner->vecPosition.x;
    widgetBox.y -= owner->vecPosition.y;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox({0, 0}, owner->vecTransformedSize);
    // crashes with xray true
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, Config::panelBaseColor); // need blurfbshouldrender = true
    g_pHyprOpenGL->m_RenderData.clipBox = CBox();

    std::vector<int> workspaces;

    if (Config::showSpecialWorkspace) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    int lowestID = INFINITY;
    int highestID = 1;
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (!ws) continue;
        // normal workspaces start from 1, special workspaces ends on -2
        if (ws->m_iID < 1) continue;
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
        if (hyprsplitNumWorkspaces > 0) {
            wsIDStart = std::min<int>(hyprsplitNumWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(hyprsplitNumWorkspaces * ownerID + 1, highestID); // always show the initial workspace for current monitor
        }
        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            if (i == owner->activeSpecialWorkspaceID()) continue;
            const auto pWorkspace = g_pCompositor->getWorkspaceByID(i);
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    // add new empty workspace
    if (Config::showNewWorkspace) {
        // get the lowest empty workspce id after the highest id of current workspace
        while (g_pCompositor->getWorkspaceByID(highestID) != nullptr) highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());

    // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = ((Config::panelHeight - 2 * Config::workspaceMargin) / (owner->vecTransformedSize.y)) * owner->scale; // scale box with panel height
    double workspaceBoxW = owner->vecTransformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->vecTransformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + (Config::workspaceMargin * owner->scale) * (wsCount - 1);
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset.value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset.value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->scale) - curYOffset.value()) : (owner->vecTransformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->scale) - workspaceBoxH + curYOffset.value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (Config::workspaceMargin * owner->scale), 0);

    workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset.goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};
        if (ws == owner->activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0) {
                g_pHyprOpenGL->renderBorder(&curWorkspaceBox, CGradientValueData(Config::workspaceActiveBorder), 0, Config::workspaceBorderSize);
            }
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, Config::workspaceActiveBackground); // cant really round it until I find a proper way to clip windows to a rounded rect
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->scale);
                continue;
            }
        }
        else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0) {
                g_pHyprOpenGL->renderBorder(&curWorkspaceBox, CGradientValueData(Config::workspaceInactiveBorder), 0, Config::workspaceBorderSize);
            }
            g_pHyprOpenGL->renderRectWithBlur(&curWorkspaceBox, Config::workspaceInactiveBackground);
        }

        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.get(), owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
            for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.get(), owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }

        if (owner->activeWorkspace == ws) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            if (Config::onBottom) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            g_pHyprOpenGL->renderRectWithBlur(&miniPanelBox, CColor(0, 0, 0, 0), 0, 1.f, false);
        }

        if (ws != nullptr) {

            // draw tiled windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && !w->m_bIsFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
                    renderWindowStub(w.get(), owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && w->m_bIsFloating && ws->getLastFocusedWindow() != w.get()) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
                    renderWindowStub(w.get(), owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_bIsFloating) {
                    CWindow* w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition.value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition.value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize.value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize.value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
        }

        if (owner->activeWorkspace != ws || !Config::hideRealLayers) {
            // this layer is hidden for real workspace when panel is displayed
            if (!Config::hideTopLayers)
                for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.get(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.get(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
        }

        // resets workspaceBox to absolute position for input detection
        curWorkspaceBox.x += owner->vecPosition.x;
        curWorkspaceBox.y += owner->vecPosition.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->scale;
    }
}
