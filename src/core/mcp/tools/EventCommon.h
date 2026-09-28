/*
 * Xournal++ (xournalai)
 *
 * JSON form of document events (shared by changes_get, wait_for_user and notifications)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "api/EventHub.h"
#include "mcp/Json.h"

namespace xoj::mcp::tools {

json eventJson(const api::DocEvent& e);

}  // namespace xoj::mcp::tools
