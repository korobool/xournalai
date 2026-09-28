#include "Tools.h"

namespace xoj::mcp::tools {

void registerAll(McpServer& server) {
    registerStatusTools(server);
    registerReadTools(server);
    registerRenderTools(server);
    registerLayoutTools(server);
    registerGuideTools(server);
    registerFileTools(server);
}

}  // namespace xoj::mcp::tools
