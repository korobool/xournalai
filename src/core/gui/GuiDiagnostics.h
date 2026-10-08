/*
 * xournalai (based on Xournal++)
 *
 * Evidence for crashes in GTK's own code (2026-10-08: GtkWindow positioned a popover that no longer existed, and the
 * app crashed). With the stall trace on, every destroyed popover is written down (its name, what it points at,
 * whether it was still shown), and the first "not a widget" error from GTK gets this thread's stack: the next such
 * crash names the popover and who destroyed it.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

/// Call once on the UI thread
void installGuiDiagnostics();
