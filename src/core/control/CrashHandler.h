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
/// (xournalai) The UI is frozen: save the open document to the emergency file (offered on the next start), from
/// another thread. Reads under the document's shared lock (waits up to 3 s for it); never touches GTK.
bool rescueSave();
/// A clean quit: a rescue file written by this run is not needed any more
void discardRescue();
