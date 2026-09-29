/*
 * Xournal++
 *
 * Error handler, prints a stacktrace if Xournal++ crashes
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class Document;
void setEmergencyDocument(const Document* doc);
void installCrashHandlers();
/// Once a GLib main loop runs (the GUI): handle SIGTERM/SIGINT there instead of in a signal handler (unix)
void handleCloseSignalsInMainLoop();
void emergencySave();
