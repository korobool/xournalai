/*
 * xournalai (based on Xournal++)
 *
 * When the X server stops answering this app (seen twice on 2026-10-05: the UI thread waited forever for a reply
 * that never came, while the server served everyone else), nothing inside the app can unblock it. After 15 s the
 * watchdog rescues the document (offered on the next start); if the UI thread is waiting on X then, it closes the X
 * connection: GTK reports the lost display and the app exits, instead of staying frozen until killed.
 * HANG lines in the stall trace also get the X request counters (last sent / last the server answered), to tell a
 * silent server from a reply lost inside the app.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for uint64_t
#include <string>   // for string
#include <vector>   // for vector

/// Call once on the UI thread, before the stall watchdog starts. No-op off X11.
void installXHangRecovery();

/// The requests the X request log has noted so far (0: the log is off, and HANG reports cannot name requests)
uint64_t xRequestsLogged();

/// The X requests sent since the start, by kind (e.g. "PutImage", "RENDER:8"): how many, and their bytes. For
/// finding what loads the X server (2026-10-20: ~40 000 requests a second while an AI zone was animated)
struct XRequestKind {
    std::string name;
    uint64_t count;
    uint64_t bytes;
};
std::vector<XRequestKind> xRequestTotals();

/// X errors logged and dropped instead of ending the app (RENDER's and BadAlloc; GDK ends it on the others)
uint64_t xErrorsSurvivedCount();
/// (Tests) Sends a RENDER request that the server answers with an error; false off X11
bool provokeRenderError();
