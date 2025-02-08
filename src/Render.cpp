#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

// What are we even doing...
class CFakeDamageElement : public IPassElement {
public:
    CBox       box;

    CFakeDamageElement(const CBox& box) {
        this->box = box;
    }
    virtual ~CFakeDamageElement() = default;

    virtual void                draw(const CRegion& damage) {
        return;
    }
    virtual bool                needsLiveBlur() {
        return true; // hack
    }
    virtual bool                needsPrecomputeBlur() {
        return false;
    }
    virtual std::optional<CBox> boundingBox() {
        return box.copy().scale(1.f / g_pHyprOpenGL->m_RenderData.pMonitor->scale).round();
    }
    virtual CRegion             opaqueRegion() {
        return CRegion{};
    }
    virtual const char* passName() {
        return "CFakeDamageElement";
    }

};


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, CGradientValueData gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = 0;
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CBorderPassElement>(data));
}

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, timespec* time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride || !time) return;

    SRenderModifData renderModif;

    const auto oWorkspace = pWindow->m_pWorkspace;
    const auto oFullscreen = pWindow->m_sFullscreenState;
    const auto oRealPosition = pWindow->m_vRealPosition->value();
    const auto oSize = pWindow->m_vRealSize->value();
    const auto oUseNearestNeighbor = pWindow->m_sWindowData.nearestNeighbor;
    const auto oPinned = pWindow->m_bPinned;
    const auto oDraggedWindow = g_pInputManager->currentlyDraggedWindow;
    const auto oDragMode = g_pInputManager->dragMode;
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;
    const auto oFloating = pWindow->m_bIsFloating;

    const float curScaling = rectOverride.w / (oSize.x * pMonitor->scale);

    // using renderModif struct to override the position and scale of windows
    // this will be replaced by matrix transformations in hyprland
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, (pMonitor->vecPosition * pMonitor->scale) + (rectOverride.pos() / curScaling) - (oRealPosition * pMonitor->scale)});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;
    pWindow->m_pWorkspace = pWorkspaceOverride;
    pWindow->m_sFullscreenState = SFullscreenState{FSMODE_NONE};
    pWindow->m_sWindowData.nearestNeighbor = false;
    pWindow->m_bIsFloating = false;
    pWindow->m_bPinned = true;
    pWindow->m_sWindowData.rounding = CWindowOverridableVar<int>(pWindow->rounding() * curScaling * pMonitor->scale, eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->currentlyDraggedWindow = pWindow; // override these and force INTERACTIVERESIZEINPROGRESS = true to trick the renderer
    g_pInputManager->dragMode = MBIND_RESIZE;

    g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    // remove modif as it goes out of scope (wtf is this blackmagic i need to relearn c++)
    Hyprutils::Utils::CScopeGuard x([&renderModif] {
        g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        });

    g_pHyprRenderer->damageWindow(pWindow);

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, RENDER_PASS_ALL, false, false);

    // restore values for normal window render
    pWindow->m_pWorkspace = oWorkspace;
    pWindow->m_sFullscreenState = oFullscreen;
    pWindow->m_sWindowData.nearestNeighbor = oUseNearestNeighbor;
    pWindow->m_bIsFloating = oFloating;
    pWindow->m_bPinned = oPinned;
    pWindow->m_sWindowData.rounding.unset(eOverridePriority::PRIORITY_SET_PROP);
    g_pInputManager->currentlyDraggedWindow = oDraggedWindow;
    g_pInputManager->dragMode = oDragMode;
}

void renderLayerStub(PHLLS pLayer, PHLMONITOR pMonitor, CBox rectOverride, timespec* time) {
    if (!pLayer || !pMonitor || !time) return;

    if (!pLayer->mapped || pLayer->readyToDelete || !pLayer->layerSurface) return;

    Vector2D oRealPosition = pLayer->realPosition->value();
    Vector2D oSize = pLayer->realSize->value();
    float oAlpha = pLayer->alpha->value(); // set to 1 to show hidden top layer
    const auto oRenderModifEnable = g_pHyprOpenGL->m_RenderData.renderModif.enabled;
    const auto oFadingOut = pLayer->fadingOut;

    const float curScaling = rectOverride.w / (oSize.x);

    SRenderModifData renderModif;

    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, pMonitor->vecPosition + (rectOverride.pos() / curScaling) - oRealPosition});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, curScaling});
    renderModif.enabled = true;
    pLayer->alpha->setValue(1);
    pLayer->fadingOut = false;

    g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{renderModif}));
    // remove modif as it goes out of scope (wtf is this blackmagic i need to relearn c++)
    Hyprutils::Utils::CScopeGuard x([&renderModif] {
        g_pHyprRenderer->m_sRenderPass.add(makeShared<CRendererHintsPassElement>(CRendererHintsPassElement::SData{SRenderModifData{}}));
        });

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false);

    pLayer->fadingOut = oFadingOut;
    pLayer->alpha->setValue(oAlpha);
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck? xd
void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset->isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFBShouldRender = true;

    int bottomInvert = 1;
    if (Config::onBottom) bottomInvert = -1;

    // Background box
    CBox widgetBox = {owner->vecPosition.x, owner->vecPosition.y + (Config::onBottom * (owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->scale))) - (bottomInvert * curYOffset->value()), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale}; //TODO: update size on monitor change

    // set widgetBox relative to current monitor for rendering panel
    widgetBox.x -= owner->vecPosition.x;
    widgetBox.y -= owner->vecPosition.y;

    g_pHyprOpenGL->m_RenderData.clipBox = CBox({0, 0}, owner->vecTransformedSize);

    if (!Config::disableBlur) {
        renderRectWithBlur(widgetBox, Config::panelBaseColor);
    }
    else {
        renderRect(widgetBox, Config::panelBaseColor);
    }

    // Panel Border
    if (Config::panelBorderWidth > 0) {
        // Border box
        CBox borderBox = {widgetBox.x, owner->vecPosition.y + (Config::onBottom * owner->vecTransformedSize.y) + (Config::panelHeight + Config::reservedArea - curYOffset->value() * owner->scale) * bottomInvert, owner->vecTransformedSize.x, Config::panelBorderWidth};
        borderBox.y -= owner->vecPosition.y;

        renderRect(borderBox, Config::panelBorderColor);
    }


    // unscaled and relative to owner
    //CBox damageBox = {0, (Config::onBottom * (owner->vecTransformedSize.y - ((Config::panelHeight + Config::reservedArea)))) - (bottomInvert * curYOffset->value()), owner->vecTransformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->scale};

    //owner->addDamage(damageBox);
    g_pHyprRenderer->damageMonitor(owner);

    // add a fake element with needsliveblur = true and covers the entire monitor to ensure damage applies to the entire monitor
    // unoptimized atm but hey its working
    CFakeDamageElement fakeDamage = CFakeDamageElement(CBox({0, 0}, owner->vecTransformedSize));
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CFakeDamageElement>(fakeDamage));

    // the list of workspaces to show
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

    // include empty workspaces that are between non-empty ones
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

    // add a new empty workspace at last
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
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset->value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset->value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->scale) - curYOffset->value()) : (owner->vecTransformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->scale) - workspaceBoxH + curYOffset->value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (Config::workspaceMargin * owner->scale), 0);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};

        // workspace background rect (NOT background layer) and border
        if (ws == owner->activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceActiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceActiveBackground); // cant really round it until I find a proper way to clip windows to a rounded rect
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceActiveBackground);
            }
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->scale);
                continue;
            }
        }
        else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, CGradientValueData(Config::workspaceInactiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
        }

        // background and bottom layers
        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_aLayerSurfaceLayers[0]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition->value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
            for (auto& ls : owner->m_aLayerSurfaceLayers[1]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition->value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize->value() * monitorSizeScaleFactor};
                g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, &time);
                g_pHyprOpenGL->m_RenderData.clipBox = CBox();
            }
        }

        // the mini panel to cover the awkward empty space reserved by the panel
        if (owner->activeWorkspace == ws && Config::affectStrut) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            if (Config::onBottom) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            if (!Config::disableBlur) {
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }
            else {
                // what
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }

        }

        if (ws != nullptr) {
            // draw tiled windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && !w->m_bIsFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition->value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition->value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize->value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize->value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w) continue;
                if (w->m_pWorkspace == ws && w->m_bIsFloating && ws->getLastFocusedWindow() != w) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition->value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition->value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize->value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize->value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_bIsFloating) {
                    const auto w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + ((w->m_vRealPosition->value().x - owner->vecPosition.x) * monitorSizeScaleFactor * owner->scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_vRealPosition->value().y - owner->vecPosition.y) * monitorSizeScaleFactor * owner->scale);
                    double wW = w->m_vRealSize->value().x * monitorSizeScaleFactor * owner->scale;
                    double wH = w->m_vRealSize->value().y * monitorSizeScaleFactor * owner->scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->activeWorkspace, curWindowBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }
        }

        if (owner->activeWorkspace != ws || !Config::hideRealLayers) {
            // this layer is hidden for real workspace when panel is displayed
            if (!Config::hideTopLayers)
                for (auto& ls : owner->m_aLayerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition->value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
                    g_pHyprOpenGL->m_RenderData.clipBox = CBox();
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_aLayerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->realPosition->value() - owner->vecPosition) * monitorSizeScaleFactor, ls->realSize->value() * monitorSizeScaleFactor};
                    g_pHyprOpenGL->m_RenderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, &time);
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
