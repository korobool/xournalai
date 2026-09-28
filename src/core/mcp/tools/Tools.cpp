#include "Tools.h"

namespace xoj::mcp::tools {

void registerAll(McpServer& server) {
    registerStatusTools(server);
    registerReadTools(server);
    registerRenderTools(server);
    registerLayoutTools(server);
    registerGuideTools(server);
    registerFileTools(server);
    registerExportTools(server);
    registerDrawTools(server);
    registerContentTools(server);
    registerSvgTools(server);
    registerPlacementTools(server);
    registerPenTools(server);
    registerDraftTools(server);
    registerEditTools(server);
    registerImportTools(server);
    registerActionTools(server);
    registerStructureTools(server);
    registerControlTools(server);
    registerUiTools(server);
    registerMenuTools(server);
}

}  // namespace xoj::mcp::tools
