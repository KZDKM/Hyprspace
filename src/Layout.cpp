#include "Overview.hpp"
#include "Globals.hpp"

void CHyprspaceWidget::reserveArea() {
    const auto currentHeight = Config::panelHeight - curYOffset.goal();
    const auto pMonitor = getOwner();
    if (currentHeight > pMonitor->vecReservedTopLeft.y) {
            const auto oActiveWorkspace = pMonitor->activeWorkspace;

            for (auto& ws : g_pCompositor->m_vWorkspaces) { // HACK: recalculate other workspaces without reserved area
                if (ws->m_iMonitorID == ownerID && ws->m_iID != oActiveWorkspace->m_iID) {
                    pMonitor->activeWorkspace = ws;
                    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ownerID);
                }
            }
            pMonitor->activeWorkspace = oActiveWorkspace;
            pMonitor->vecReservedTopLeft.y = currentHeight;
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ownerID);
    }
}