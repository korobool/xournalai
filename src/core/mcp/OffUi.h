/*
 * xournalai (based on Xournal++)
 *
 * Runs the heavy part of a tool off the UI thread, so agents reading the document (rendering a page to look at it,
 * serializing thousands of strokes) never freeze the user's pen. The work runs on the render thread (serialized with
 * page rendering, so PDF backgrounds are never rendered concurrently) and its result is delivered on the UI thread.
 *
 * The work must not touch GTK. It may read the document under a *short* shared lock (take a snapshot, see
 * model/PageSnapshot.h) and must do the long part without the lock.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function

#include "Registry.h"  // for ToolResult, Responder

class Control;

namespace xoj::mcp {

void runOffUi(Control* ctrl, std::function<ToolResult()> work, Responder respond);

}  // namespace xoj::mcp
