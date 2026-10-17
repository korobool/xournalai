#include "CrashHandler.h"

#include <atomic>
#include <chrono>  // for milliseconds
#include <csignal>
#include <iostream>
#include <string>
#include <thread>  // for sleep_for

#ifdef __unix__
#include <glib-unix.h>  // for g_unix_signal_add
#include <unistd.h>     // for alarm, pause
#endif

#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"               // for Document
#include "util/PathUtil.h"
#include "util/Stacktrace.h"
#include "util/VersionInfo.h"

#include "filesystem.h"  // for path

static std::atomic<const Document*> document = nullptr;
static std::atomic<int> alreadyCrashed = 0;
static std::atomic<bool> rescued = false;  // this run wrote a rescue file
static std::atomic<bool> stuckForTests = false;
/// (xournalai) A crash handler still running after this long is stuck: the process ends
constexpr unsigned CRASH_DEADLINE_S = 25;

bool crashing() { return alreadyCrashed > 0; }
void simulateStuckCrashHandler() { stuckForTests = true; }

extern "C" void forceClose(int sig) {
    g_warning("Force close requested with signal %i", sig);
    emergencySave();
    exit(1);
}

void setEmergencyDocument(const Document* doc) { document = doc; }

void emergencySave() {
    if (document == nullptr) {
        return;
    }

    std::cerr << "Trying to emergency save the current open document..." << std::endl;

    auto const& filepath = Util::getConfigFile("emergencysave.xopp");

    SaveHandler handler;
    handler.prepareSave(document, filepath);
    handler.saveTo(filepath);

    if (!handler.getErrorMessage().empty()) {
        std::cerr << "Error: " << handler.getErrorMessage() << std::endl;
    } else {
        std::cerr << "Successfully saved document to \"" << char_cast(filepath.u8string()) << "\"" << std::endl;
    }
}


bool rescueSave() {
    const Document* doc = document;
    if (!doc) {
        return false;
    }
    auto* lockable = const_cast<Document*>(doc);
    bool locked = false;
    for (int i = 0; i < 300 && !(locked = lockable->try_lock_shared()); i++) {  // (a frozen writer: give up)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!locked) {
        g_warning("Rescue save: the document stays locked; not saved");
        return false;
    }
    auto const& filepath = Util::getConfigFile("emergencysave.xopp");
    SaveHandler handler;
    handler.prepareSave(doc, filepath);
    lockable->unlock_shared();
    handler.saveTo(filepath);
    if (!handler.getErrorMessage().empty()) {
        g_warning("Rescue save failed: %s", handler.getErrorMessage().c_str());
        return false;
    }
    rescued = true;
    g_message("The UI is frozen: rescued the document to %s", filepath.string().c_str());
    return true;
}

void discardRescue() {
    if (rescued.exchange(false)) {
        std::error_code ec;
        fs::remove(Util::getConfigFile("emergencysave.xopp"), ec);
    }
}

/// Print a backtrace and try to make an emergency save
extern "C" void crashHandler(int sig) {
    int crash = ++alreadyCrashed;
#ifdef __unix__
    if (crash == 1) {
        // (xournalai) After heap corruption the allocator's lock stays held and the allocations below wait for it
        // forever: the dead app kept its window, its MCP port (the next start's AI terminal could not connect) and its
        // helpers (2026-10-17). A deadline ends it; the hang watchdog rescues the document and ends it sooner.
        std::signal(SIGALRM, SIG_DFL);
        alarm(CRASH_DEADLINE_S);
    }
#endif

    std::cerr << "\n\n\n*************************************************************************\n\n";
    std::cerr << "[Crash Handler] Crashed " << crash << " time(s) with signal " << sig << std::endl;
    if (crash == 1) {  // Avoid a loop in case we crash again on emergencySave()
#ifdef __unix__
        while (stuckForTests) {  // (where the real one waited: the version info allocates)
            pause();
        }
#endif
        std::cerr << xoj::util::getVersionInfo() << std::endl;

        std::cerr << "\nTry to get a stacktrace...\n";

        Stacktrace::printStacktrace(std::cerr);
        std::cerr << std::endl;

        emergencySave();
    }

#ifdef __unix__
    // Forward the signal for the system's default handling - we may get a coredump
    std::signal(sig, SIG_DFL);
    kill(getpid(), sig);
#endif

    exit(crash);
}

#ifdef __unix__
/// SIGTERM/SIGINT are handled by the main loop: saving from inside a signal handler deadlocks when the signal
/// interrupts code holding a lock (e.g. the allocator's), leaving a hung process.
static gboolean forceCloseFromMainLoop(gpointer sig) {
    forceClose(GPOINTER_TO_INT(sig));
    return G_SOURCE_REMOVE;
}
#endif

void handleCloseSignalsInMainLoop() {
#ifdef __unix__
    g_unix_signal_add(SIGTERM, forceCloseFromMainLoop, GINT_TO_POINTER(SIGTERM));
    g_unix_signal_add(SIGINT, forceCloseFromMainLoop, GINT_TO_POINTER(SIGINT));
#endif
}

void installCrashHandlers() {
    std::signal(SIGTERM, forceClose);
    std::signal(SIGINT, forceClose);
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGFPE, crashHandler);
    std::signal(SIGILL, crashHandler);
    std::signal(SIGABRT, crashHandler);
#ifdef SIGTRAP
    std::signal(SIGTRAP, crashHandler);
#endif
}
