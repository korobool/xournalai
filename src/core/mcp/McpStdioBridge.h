/*
 * xournalai (based on Xournal++)
 *
 * stdio <-> HTTP bridge for MCP clients that can only launch stdio servers
 *
 * @license GNU GPLv2 or later
 */

#pragma once

namespace xoj::mcp {

/**
 * @brief Runs `xournalpp --mcp-stdio`.
 *
 * Reads newline-delimited JSON-RPC messages from stdin, forwards each one to the MCP endpoint of the running
 * xournalai instance (starting it if necessary) and writes the responses and server notifications to stdout, one
 * message per line. Logs go to stderr. Returns when stdin is closed.
 *
 * @param executable path of the xournalpp binary, used to start the application if it is not running
 * @return process exit code
 */
int runStdioBridge(const char* executable);

}  // namespace xoj::mcp
