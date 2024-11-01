#include "Overview.hpp"
#include "Globals.hpp"

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    const auto oWorkspace = pWindow->m_pWorkspace;
    const auto oFullscreen = pWindow->m_sFullscreenState;
    const auto oRealPosition = pWindow->m_vRealPosition.value();
    const auto oSize = pWindow->m_vRealSize.value();
    const auto oUseNearestNeighbor = pWindow->m_sWindowData.nearestNeighbor;
    const auto oPinned = pWindow->m_bPinned;
    const auto oDraggedWindow = g_pInputManager->currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->dragMode;
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;
    const auto oFloating = pWindow->m_bIsFloating;

    const float curScaling = rectOverride.w / (oSize.x * pMonitor->scale);

    // using renderModif struct to override the position and scale of windows
    // this will be replaced by matrix transformations in hyprland
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, (pMonitor->vecPosition * pMonitor->scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->scale)});
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = true;
    pWindow->m_pWorkspace = pWorkspaceOverride;
    pWindow->m_sFullscreenState = sFullscreenState{FSMODE_NONE}; // FIXME: still do nothing, fullscreen requests not reject when overview active
    pWindow->m_sWindowData.nearestNeighbor = false; // FIX: this wont do, need to scale surface texture down properly so that windows arent shown as pixelated mess
    pWindow->m_bIsFloating = false; // weird shit happened so hack fix
    pWindow->m_bPinned = true;
    pWindow->m_sWindowData.rounding = CWindowOverridableVar<int>(pWindow->rounding() * curScaling * pMonitor->scale, eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->currentlyDraggedWindow = pWindow; // override these and force INTERACTIVERESIZEINPROGRESS = true to trick the renderer
    g_pInputManager->dragMode = MBIND_RESIZE;

    g_pHyprRenderer->damageWindow(pWindow);

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_MAIN, false, false);

    // restore values for normal window render
    pWindow->m_pWorkspace = oWorkspace;
    pWindow->m_sFullscreenState = oFullscreen;
    pWindow->m_sWindowData.nearestNeighbor = oUseNearestNeighbor;
    pWindow->m_bIsFloating = oFloating;
    pWindow->m_bPinned = oPinned;
    pWindow->m_sWindowData.rounding.unset(eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->dragMode = oDragMode;
    g_pHyprOpenGL->m_RenderData.renderModif.enabled = oRenderModifEnable;
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
    g_pHyprOpenGL->m_RenderData.renderModif.modifs.pop_back();
}

void renderLayerStub(Hyprutils::Memory::CWeakPointer<CLayerSurface> pLayer, PHLMONITOR pMonitor, CBox rectOverride, timespec* time) {
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

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck? xd
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

    int bottomInvert = 1;
    if (Config::onBottom) bottomInvert = -1;

    // Background box
    CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y + (Config::onBottom * (owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->scale))) - (bottomInvert * curYOffset.value()), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale}; //TODO: update size on monitor change

    // set widgetBox relative to current monitor for rendering panel
    widgetBox.x -= owner->vecPosition.x;
    widgetBox.y -= owner->vecPosition.y;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox({0, 0}, owner->vecTransformedSize);
    g_pHyprOpenGL->renderRectWithBlur(&widgetBox, Config::panelBaseColor);

    // Panel Border
    if (Config::panelBorderWidth > 0) {
        // Border box
        CBox borderBox = {widgetBox.x, owner->vecPosition.y + (Config::onBottom * owner->vecTransformedSize.y) + (Config::panelHeight + Config::reservedArea - curYOffset.value() * owner->scale) * bottomInvert, owner->vecTransformedSize.x, Config::panelBorderWidth};
        borderBox.y -= owner->vecPosition.y;

        g_pHyprOpenGL->renderRect(&borderBox, Config::panelBorderColor);
    }


    g_pHyprRenderer->damageBox(&widgetBox);

    g_pHyprOpenGL->m_RenderData.clipBox = CBox();

    std::vector<int> workspaces;

    if (Config::showSpecialWorkspace) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    // find the lowest and highest workspace id to determine which empty workspaces to insert
    int lowestID = INT_MAX;
    int highestID = 1;
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (!ws) continue;
        // normal workspaces start from 1, special workspaces ends on -2
        if (ws->m_iID < 1) continue;
        if (ws->m_pMonitor->ID == ownerID) {
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

        // workspace background rect (NOT background layer) and border
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

        // background and bottom layers
        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_aLayerSurfaceLayers[0]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls, owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
            for (auto& ls : owner->m_aLayerSurfaceLayers[1]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls, owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }

        // the mini panel to cover the awkward empty space reserved by the panel
        if (owner->activeWorkspace == ws && Config::affectStrut) {
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
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && w->m_bIsFloating && ws->getLastFocusedWindow() != w) {
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
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_bIsFloating) {
                    const auto w = ws->getLastFocusedWindow();
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
                for (auto& ls : owner->m_aLayerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls, owner, layerBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_aLayerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition.value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize.value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls, owner, layerBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
        }

        // Resets workspaceBox to scaled absolute coordinates for input detection.
        // While rendering is done in pixel coordinates, input detection is done in
        // scaled coordinates, taking into account monitor scaling.
        // Since the monitor position is already given in scaled coordinates,
        // we only have to scale all relative coordinates, then add them to the
        // monitor position to get a scaled absolute position.
        curWorkspaceBox.scale(1 / owner->scale);

        curWorkspaceBox.x += owner->vecPosition.x;
        curWorkspaceBox.y += owner->vecPosition.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        // set the current position to the next workspace box
        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->scale;
    }
}
