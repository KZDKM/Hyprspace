#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>

static void applyGapsRule(const WORKSPACEID& id, const Config::CCssGapData& gapsIn, const Config::CCssGapData& gapsOut) {
    Config::CWorkspaceRule rule;

    // replaceOrAdd() and getWorkspaceRuleFor() match on m_workspaceString, so it has to
    // be the same identifier text the old parser would have seen
    rule.m_workspaceString            = std::to_string(id);
    const auto& [wsID, wsName, auto_] = getWorkspaceIDNameFromString(rule.m_workspaceString);
    rule.m_workspaceName              = wsName;
    rule.m_workspaceId                = auto_ ? WORKSPACE_INVALID : wsID;

    rule.m_gapsIn                     = gapsIn;
    rule.m_gapsOut                    = gapsOut;

    Config::workspaceRuleMgr()->replaceOrAdd(std::move(rule));
}

// FIXME: preserve original workspace rules
void CHyprspaceWidget::updateLayout() {

    if (!config.affectStrut->value()) return;

    const auto currentHeight = config.panelHeight->value() + config.reservedArea->value();
    const auto pMonitor = getOwner();
    if (!pMonitor) return;

    static auto PGAPSINDATA  = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    if (!PGAPSINDATA.good() || !PGAPSOUTDATA.good()) return;

    const auto PGAPSINBASE  = PGAPSINDATA.ptr();
    const auto PGAPSOUTBASE = PGAPSOUTDATA.ptr();
    if (!PGAPSINBASE || !PGAPSOUTBASE || PGAPSINBASE->getDataType() != Config::CVD_TYPE_CSS_VALUE || PGAPSOUTBASE->getDataType() != Config::CVD_TYPE_CSS_VALUE) return;

    auto* const PGAPSIN  = static_cast<Config::CCssGapData*>(PGAPSINBASE);
    auto* const PGAPSOUT = static_cast<Config::CCssGapData*>(PGAPSOUTBASE);

    if (active) {
        if (!config.onBottom->value())
            pMonitor->m_reservedArea = Desktop::CReservedArea(currentHeight, 0, 0, 0);
        else
            pMonitor->m_reservedArea = Desktop::CReservedArea(0, 0, currentHeight, 0);
    } else {
        pMonitor->m_reservedArea = Desktop::CReservedArea();
    }

    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);

    // gaps are created via workspace rules
    // there are no way to write to m_dWorkspaceRules directly
    // and we want to refrain from using function hooks
    // so we create a workspace rule for ALL workspaces through handleWorkspaceRules
    // Geneva Convention violation type hack but idc atm
    if (active) {
        const auto oActiveWorkspace = pMonitor->m_activeWorkspace;
        if (!oActiveWorkspace) return;

        for (auto& ws : State::workspaceState()->workspaces()) { // HACK: recalculate other workspaces without reserved area
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID && ws->m_id != oActiveWorkspace->m_id) {
                pMonitor->m_activeWorkspace = ws.lock();
                if (config.overrideGaps->value()) {
                    applyGapsRule(pMonitor->activeWorkspaceID(), *PGAPSIN, *PGAPSOUT);
                }
                g_layoutManager->recalculateMonitor(pMonitor);
            }
        }
        pMonitor->m_activeWorkspace = oActiveWorkspace;

        const auto curRules = std::to_string(pMonitor->activeWorkspaceID()) + ", gapsin:" + std::to_string(config.gapsIn->value()) + ", gapsout:" + std::to_string(config.gapsOut->value());
        if (config.overrideGaps->value()) {
            applyGapsRule(pMonitor->activeWorkspaceID(), config.gapsIn->value(), config.gapsOut->value());
        }
        g_layoutManager->recalculateMonitor(pMonitor);

    }
    else {
        for (auto& ws : State::workspaceState()->workspaces()) {
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID) {
                if (config.overrideGaps->value()) {
                    applyGapsRule(ws->m_id, *PGAPSIN, *PGAPSOUT);
                }
            }
        }
        g_layoutManager->recalculateMonitor(pMonitor);
    }
}
