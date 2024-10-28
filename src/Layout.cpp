#include "Overview.hpp"
#include "Globals.hpp"

// FIXME: preserve original workspace rules
void CHyprspaceWidget::updateLayout() {

    if (!Config::affectStrut) return;

    const auto currentHeight = Config::panelHeight + Config::reservedArea;
    const auto pMonitor = getOwner();

    // reset reserved areas
    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);

    static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSIN = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
    auto* const PGAPSOUT = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();

    // gaps are created via workspace rules
    // there are no way to write to m_dWorkspaceRules directly
    // and we want to refrain from using function hooks
    // so we create a workspace rule for ALL workspaces through handleWorkspaceRules
    // Geneva Convention violation type hack but idc atm
    if (active) {
        const auto oActiveWorkspace = pMonitor->activeWorkspace;

        for (auto& ws : g_pCompositor->m_vWorkspaces) { // HACK: recalculate other workspaces without reserved area
            if (ws->m_pMonitor->ID == ownerID && ws->m_iID != oActiveWorkspace->m_iID) {
                pMonitor->activeWorkspace = ws;
                const auto curRules = std::to_string(pMonitor->activeWorkspaceID()) + ", gapsin:" + PGAPSIN->toString() + ", gapsout:" + PGAPSOUT->toString();
                if (Config::overrideGaps) g_pConfigManager->handleWorkspaceRules("", curRules);
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ownerID);
            }
        }
        pMonitor->activeWorkspace = oActiveWorkspace;
        if (!Config::onBottom)
            pMonitor->vecReservedTopLeft.y = currentHeight;
        else
            pMonitor->vecReservedBottomRight.y = currentHeight;
        const auto curRules = std::to_string(pMonitor->activeWorkspaceID()) + ", gapsin:" + std::to_string(Config::gapsIn) + ", gapsout:" + std::to_string(Config::gapsOut);
        if (Config::overrideGaps) g_pConfigManager->handleWorkspaceRules("", curRules);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ownerID);

    }
    else {
        for (auto& ws : g_pCompositor->m_vWorkspaces) {
            if (ws->m_pMonitor->ID == ownerID) {
                const auto curRules = std::to_string(ws->m_iID) + ", gapsin:" + PGAPSIN->toString() + ", gapsout:" + PGAPSOUT->toString();
                if (Config::overrideGaps) g_pConfigManager->handleWorkspaceRules("", curRules);
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ownerID);
            }
        }
    }
}
