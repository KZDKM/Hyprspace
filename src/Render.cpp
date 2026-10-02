#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <algorithm>
#include <climits>


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, const Config::CGradientValueData& gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = 0;
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, CBox clipBox, const Time::steady_tp& time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride) return;
    if (!pWindow->m_isMapped || !pWindow->wlSurface() || !pWindow->wlSurface()->resource()) return;

    Render::SRenderModifData renderModif;

    const auto oRealPosition = pWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const auto oSize = pWindow->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const float    logicalW = std::max((float)oSize.x, 5.F);
    const float    scaleMod = rectOverride.w / std::max(logicalW * pMonitor->m_scale, 5.F);
    if (!(scaleMod > 0.F) || !(rectOverride.w > 0 && rectOverride.h > 0)) return;

    const Vector2D logicalTL = oRealPosition + pWindow->m_floatingOffset;
    const Vector2D scaledTL  = (logicalTL - pMonitor->m_position) * pMonitor->m_scale;
    const Vector2D translate = rectOverride.pos() / scaleMod - scaledTL;

    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(translate)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(scaleMod)));
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });

    g_pHyprRenderer->damageWindow(pWindow);

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time};
    renderdata.pos                  = oRealPosition + pWindow->m_floatingOffset;
    renderdata.w                    = std::max(oSize.x, 5.0);
    renderdata.h                    = std::max(oSize.y, 5.0);
    renderdata.surface              = pWindow->wlSurface()->resource();
    renderdata.dontRound            = Fullscreen::controller()->isFullscreen(pWindow, Fullscreen::FSMODE_FULLSCREEN);
    renderdata.fadeAlpha            = 1.F;
    renderdata.alpha                = 0.999F;
    renderdata.decorate             = false;
    renderdata.rounding             = renderdata.dontRound ? 0 : pWindow->rounding() * scaleMod * pMonitor->m_scale;
    renderdata.roundingPower        = renderdata.dontRound ? 2.0F : pWindow->roundingPower();
    renderdata.blur                 = false;
    renderdata.pWindow              = pWindow;
    renderdata.clipBox              = clipBox;
    renderdata.useNearestNeighbor   = false;
    renderdata.squishOversized      = true;
    renderdata.surfaceCounter       = 0;

    pWindow->wlSurface()->resource()->breadthfirst(
        [&renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s || !s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pWindow->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        nullptr);
}

void renderLayerStub(PHLLS pLayer, PHLMONITOR pMonitor, CBox rectOverride, CBox clipBox, const Time::steady_tp& time) {
    if (!pLayer || !pMonitor) return;

    if (!pLayer->m_mapped || !pLayer->m_layerSurface || !pLayer->wlSurface() || !pLayer->wlSurface()->resource()) return;

    Vector2D oRealPosition = pLayer->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    Vector2D oSize = pLayer->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);

    const float curScaling = rectOverride.w / (oSize.x);
    if (!(curScaling > 0.F) || !(rectOverride.w > 0 && rectOverride.h > 0)) return;

    Render::SRenderModifData renderModif;

    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(pMonitor->m_position + (rectOverride.pos() / curScaling) - oRealPosition)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(curScaling)));
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time, oRealPosition};
    renderdata.fadeAlpha                        = 1.F;
    renderdata.alpha                            = 0.999F;
    renderdata.blur                             = false;
    renderdata.surface                          = pLayer->wlSurface()->resource();
    renderdata.decorate                         = false;
    renderdata.w                                = oSize.x;
    renderdata.h                                = oSize.y;
    renderdata.pLS                              = pLayer;
    renderdata.clipBox                          = clipBox;
    renderdata.surfaceCounter                   = 0;

    pLayer->wlSurface()->resource()->breadthfirst(
        [&renderdata, &pLayer](SP<CWLSurfaceResource> s, const Vector2D& offset, void* data) {
            if (!s || !s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pLayer->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        &renderdata);
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck? xd
void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset->isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    // Full-monitor clip in monitor-local coords. Never use default CBox() to "clear" clipBox —
    // hyprutils::CBox() only sets w/h to 0 and leaves x/y uninitialized, which corrupts scissor state.
    const CBox monitorClip = {{0, 0}, owner->m_transformedSize};

    const auto time = Time::steadyNow();

    owner->m_blurFBShouldRender = true;

    int bottomInvert = 1;
    if (config.onBottom->value()) bottomInvert = -1;

    // Background box
    CBox widgetBox = {owner->m_position.x, owner->m_position.y + (config.onBottom->value() * (owner->m_transformedSize.y - ((config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale}; //TODO: update size on monitor change

    // set widgetBox relative to current monitor for rendering panel
    widgetBox.x -= owner->m_position.x;
    widgetBox.y -= owner->m_position.y;

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;

    if (!config.disableBlur->value()) {
        renderRectWithBlur(widgetBox, config.panelBaseColor->value());
    }
    else {
        renderRect(widgetBox, config.panelBaseColor->value());
    }

    // Panel Border
    if (config.panelBorderWidth->value() > 0) {
        // Border box
        CBox borderBox = {widgetBox.x, owner->m_position.y + (config.onBottom->value() * owner->m_transformedSize.y) + (config.panelHeight->value() + config.reservedArea->value() - curYOffset->value() * owner->m_scale) * bottomInvert, owner->m_transformedSize.x, static_cast<double>(config.panelBorderWidth->value())};
        borderBox.y -= owner->m_position.y;

        renderRect(borderBox, config.panelBorderColor->value());
    }


    // unscaled and relative to owner
    //CBox damageBox = {0, (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea)))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    //owner->addDamage(damageBox);
    g_pHyprRenderer->damageMonitor(owner);

    // damage the entire monitor to ensure full redraw during overview
    g_pHyprRenderer->damageMonitor(owner);

    // the list of workspaces to show
    std::vector<int> workspaces;

    if (config.showSpecialWorkspace->value()) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    // find the lowest and highest workspace id to determine which empty workspaces to insert
    int lowestID = INT_MAX;
    int highestID = 1;
    for (auto& ws : State::workspaceState()->workspaces()) {
        if (!ws) continue;
        // normal workspaces start from 1, special workspaces ends on -2
        if (ws->m_id < 1) continue;
        if (ws->m_monitor->m_id == ownerID) {
            workspaces.push_back(ws->m_id);
            if (highestID < ws->m_id) highestID = ws->m_id;
            if (lowestID > ws->m_id) lowestID = ws->m_id;
        }
    }

    // include empty workspaces that are between non-empty ones
    if (config.showEmptyWorkspace->value()) {
        int wsIDStart = 1;
        int wsIDEnd = highestID;

        // hyprsplit/split-monitor-workspaces compatibility
        if (numWorkspaces > 0) {
            wsIDStart = std::min<int>(numWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(numWorkspaces * ownerID + 1, highestID); // always show the initial workspace for current monitor
        }

        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            if (i == owner->activeSpecialWorkspaceID()) continue;
            const auto pWorkspace = State::workspaceState()->query().id(i).run();
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    // add a new empty workspace at last
    if (config.showNewWorkspace->value()) {
        // get the lowest empty workspce id after the highest id of current workspace
        while (State::workspaceState()->query().id(highestID).run() != nullptr) highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());

    // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = ((config.panelHeight->value() - 2 * config.workspaceMargin->value()) / (owner->m_transformedSize.y)) * owner->m_scale; // scale box with panel height
    double workspaceBoxW = owner->m_transformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->m_transformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + (config.workspaceMargin->value() * owner->m_scale) * (wsCount - 1);
    double curWorkspaceRectOffsetX = config.centerAligned->value() ? workspaceScrollOffset->value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset->value() + config.workspaceMargin->value();
    double curWorkspaceRectOffsetY = !config.onBottom->value() ? (((config.reservedArea->value() + config.workspaceMargin->value()) * owner->m_scale) - curYOffset->value()) : (owner->m_transformedSize.y - ((config.reservedArea->value() + config.workspaceMargin->value()) * owner->m_scale) - workspaceBoxH + curYOffset->value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (config.workspaceMargin->value() * owner->m_scale), 0);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = State::workspaceState()->query().id(wsID).run();
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};

        // workspace background rect (NOT background layer) and border
        if (ws == owner->m_activeWorkspace) {
            if (config.workspaceBorderSize->value() >= 1 && CHyprColor(config.workspaceActiveBorder->value()).a > 0) {
                renderBorder(curWorkspaceBox, Config::CGradientValueData(config.workspaceActiveBorder->value()), config.workspaceBorderSize->value());
            }
            if (!config.disableBlur->value()) {
                renderRectWithBlur(curWorkspaceBox, config.workspaceActiveBackground->value()); // cant really round it until I find a proper way to clip windows to a rounded rect
            }
            else {
                renderRect(curWorkspaceBox, config.workspaceActiveBackground->value());
            }
            if (!config.drawActiveWorkspace->value()) {
                curWorkspaceRectOffsetX += workspaceBoxW + (config.workspaceMargin->value() * owner->m_scale);
                continue;
            }
        }
        else {
            if (config.workspaceBorderSize->value() >= 1 && CHyprColor(config.workspaceInactiveBorder->value()).a > 0) {
                renderBorder(curWorkspaceBox, Config::CGradientValueData(config.workspaceInactiveBorder->value()), config.workspaceBorderSize->value());
            }
            if (!config.disableBlur->value()) {
                renderRectWithBlur(curWorkspaceBox, config.workspaceInactiveBackground->value());
            }
            else {
                renderRect(curWorkspaceBox, config.workspaceInactiveBackground->value());
            }
        }

        // background and bottom layers
        if (!config.hideBackgroundLayers->value()) {
            for (auto& ls : owner->m_layerSurfaceLayers[0]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) - owner->m_position) * monitorSizeScaleFactor, ls->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) * monitorSizeScaleFactor};
                renderLayerStub(ls.lock(), owner, layerBox, curWorkspaceBox, time);
            }
            for (auto& ls : owner->m_layerSurfaceLayers[1]) {
                CBox layerBox = {curWorkspaceBox.pos() + (ls->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) - owner->m_position) * monitorSizeScaleFactor, ls->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) * monitorSizeScaleFactor};
                renderLayerStub(ls.lock(), owner, layerBox, curWorkspaceBox, time);
            }
        }

        // the mini panel to cover the awkward empty space reserved by the panel
        if (owner->m_activeWorkspace == ws && config.affectStrut->value()) {
            CBox miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};
            if (config.onBottom->value()) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            if (!config.disableBlur->value()) {
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }
            else {
                // what
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }

        }

        if (ws != nullptr) {
            // draw tiled windows
            for (auto& w : Desktop::windowState()->windows()) {
                if (!w) continue;
                if (w->m_workspace == ws && !w->m_isFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, curWorkspaceBox, time);
                }
            }
            // draw floating windows
            for (auto& w : Desktop::windowState()->windows()) {
                if (!w) continue;
                if (w->m_workspace == ws && w->m_isFloating && ws->getLastFocusedWindow() != w) {
                    double wX = curWorkspaceRectOffsetX + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, curWorkspaceBox, time);
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_isFloating) {
                    const auto w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, curWorkspaceBox, time);
                }
        }

        if (owner->m_activeWorkspace != ws || !config.hideRealLayers->value()) {
            // this layer is hidden for real workspace when panel is displayed
            if (!config.hideTopLayers->value())
                for (auto& ls : owner->m_layerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) - owner->m_position) * monitorSizeScaleFactor, ls->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) * monitorSizeScaleFactor};
                    renderLayerStub(ls.lock(), owner, layerBox, curWorkspaceBox, time);
                }

            if (!config.hideOverlayLayers->value())
                for (auto& ls : owner->m_layerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) - owner->m_position) * monitorSizeScaleFactor, ls->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) * monitorSizeScaleFactor};
                    renderLayerStub(ls.lock(), owner, layerBox, curWorkspaceBox, time);
                }
        }


        // Resets workspaceBox to scaled absolute coordinates for input detection.
        // While rendering is done in pixel coordinates, input detection is done in
        // scaled coordinates, taking into account monitor scaling.
        // Since the monitor position is already given in scaled coordinates,
        // we only have to scale all relative coordinates, then add them to the
        // monitor position to get a scaled absolute position.
        curWorkspaceBox.scale(1 / owner->m_scale);

        curWorkspaceBox.x += owner->m_position.x;
        curWorkspaceBox.y += owner->m_position.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        // set the current position to the next workspace box
        curWorkspaceRectOffsetX += workspaceBoxW + config.workspaceMargin->value() * owner->m_scale;
    }

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
}
