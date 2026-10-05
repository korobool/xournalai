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

/// Call once on the UI thread, before the stall watchdog starts. No-op off X11.
void installXHangRecovery();
