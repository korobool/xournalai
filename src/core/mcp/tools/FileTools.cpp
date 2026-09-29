// Tools: file_info, file_open, file_new, file_close, file_save, file_save_as, file_recent

#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"        // for currentPageIndex
#include "control/Control.h"        // for Control
#include "control/RecentManager.h"  // for getRecentFiles
#include "control/ScrollHandler.h"  // for ScrollHandler
#include "mcp/McpServer.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Document.h"        // for Document
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

json fileInfo(Control* ctrl) {
    Document* doc = ctrl->getDocument();
    std::shared_lock lock(*doc);
    const auto path = doc->getFilepath();
    const auto pdf = doc->getPdfFilepath();
    return {{"path", path.empty() ? json(nullptr) : json(toUtf8(path))},
            {"untitled", path.empty()},
            {"pdf_background", pdf.empty() ? json(nullptr) : json(toUtf8(pdf))},
            {"modified", ctrl->getUndoRedoHandler()->isChanged()},
            {"page_count", doc->getPageCount()},
            {"current_page", api::currentPageIndex(ctrl) + 1}};
}

fs::path absolutePath(const std::string& s) {
    if (s.empty()) {
        throw ToolError("'path' must not be empty");
    }
    fs::path p = pathFromUtf8(s);
    if (!s.empty() && s[0] == '~') {
        p = fs::path(g_get_home_dir()) / pathFromUtf8(s.substr(s.size() > 1 && s[1] == '/' ? 2 : 1));
    }
    return fs::absolute(p);
}

/**
 * Deals with unsaved changes according to `policy` ("fail", "save" or "discard"), then calls `next`.
 * Calls `respond` with an error instead if the policy does not allow proceeding.
 */
void handleUnsaved(McpServer* srv, Control* ctrl, const std::string& policy, const std::string& action,
                   std::function<void()> next, const Responder& respond) {
    if (!ctrl->getUndoRedoHandler()->isChanged()) {
        next();
        return;
    }
    if (policy == "discard") {
        srv->requireTier(Tier::Destructive, "discarding unsaved changes");
        srv->backup("before-discard");
        // Not Control::resetSavedStatus(): it adds the (possibly empty) path to the recent files
        ctrl->getUndoRedoHandler()->documentSaved();
        next();
    } else if (policy == "save") {
        std::shared_lock lock(*ctrl->getDocument());
        if (ctrl->getDocument()->getFilepath().empty()) {
            throw ToolError("The current document is untitled; save it with file_save_as(path) first, or use "
                            "on_unsaved=\"discard\"");
        }
        lock.unlock();
        ctrl->save([next, respond](bool ok) {
            if (ok) {
                next();
            } else {
                respond(ToolResult::error("Saving the current document failed; nothing was " + std::string("done")));
            }
        });
    } else {
        throw ToolError("The current document has unsaved changes. Before " + action +
                        ", save it (file_save), or pass on_unsaved=\"save\" or on_unsaved=\"discard\" (discarding "
                        "needs the 'destructive' permission). Ask the user if unsure.");
    }
}

json unsavedSchema() {
    return schema::withDefault(
            schema::enumeration("What to do with unsaved changes in the current document", {"fail", "save", "discard"}),
            "fail");
}

void saveTo(Control* ctrl, const Responder& respond, std::optional<fs::path> newPath) {
    if (newPath) {
        Document* doc = ctrl->getDocument();
        doc->lock();
        doc->setCreateBackupOnSave(false);
        doc->setFilepath(*newPath);
        doc->unlock();
    }
    ctrl->save([ctrl, respond](bool ok) {
        if (!ok) {
            respond(ToolResult::error("Saving failed (see the application for details)"));
            return;
        }
        respond(ToolResult::structured(fileInfo(ctrl), "Saved."));
    });
}

}  // namespace

void registerFileTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec info;
    info.name = "file_info";
    info.title = "Current file";
    info.description =
            "Path, untitled/modified state, background PDF, page count and current page of the open document.";
    info.inputSchema = schema::object({});
    info.readOnly = true;
    info.idempotent = true;
    info.handler = [ctrl](const json&) {
        requireDocument(ctrl);
        return ToolResult::structured(fileInfo(ctrl));
    };
    server.getRegistry().addTool(std::move(info));

    ToolSpec open;
    open.name = "file_open";
    open.title = "Open file";
    open.description =
            "Opens a document in the app, replacing the current one: .xopp/.xoj notes, a .pdf to annotate (a new "
            "document with the PDF as page backgrounds, or its existing .xopp notes), a .png image as a page, or "
            "a .xopt template. If the current document has unsaved changes, on_unsaved decides (default: fail "
            "and explain). '~' expands to the home directory.";
    open.inputSchema = schema::object(
            {{"path", schema::string("File to open")},
             {"attach_pdf", schema::withDefault(schema::boolean("For PDFs: store the PDF next to/inside the notes "
                                                                "(attach mode) instead of referencing it"),
                                                false)},
             {"page", schema::integer("Page to show after opening (1-based)")},
             {"on_unsaved", unsavedSchema()}},
            {"path"});
    open.tier = Tier::Files;
    open.asyncHandler = [srv, ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"path", "attach_pdf", "page", "on_unsaved"});
        const fs::path path = absolutePath(args.str("path"));
        if (std::error_code ec; !fs::is_regular_file(path, ec)) {
            throw ToolError("File not found: " + toUtf8(path));
        }
        const bool attach = args.boolean("attach_pdf", false);
        const int page = static_cast<int>(args.integer("page", 0, 0, 100000)) - 1;
        const std::string policy = args.choice("on_unsaved", {"fail", "save", "discard"}, "fail");
        handleUnsaved(
                srv, ctrl, policy, "opening another file",
                [ctrl, path, attach, page, respond]() {
                    ctrl->openFileWithoutSavingTheCurrentDocument(
                            path, attach, page, [ctrl, path, page, respond](bool ok) {
                                if (!ok) {
                                    respond(ToolResult::error("Could not open " + toUtf8(path) +
                                                              " (the application may show details in a dialog)"));
                                    return;
                                }
                                // The app only restores the page from its per-file metadata: scroll explicitly
                                if (page >= 0) {
                                    ctrl->getScrollHandler()->scrollToPage(static_cast<size_t>(page));
                                }
                                whenReady([ctrl, page] { return page < 0 || ctrl->getCurrentPageNo() == size_t(page); },
                                          [ctrl, path, respond](bool) {
                                              respond(ToolResult::structured(fileInfo(ctrl), "Opened " + toUtf8(path)));
                                          });
                            });
                },
                respond);
    };
    server.getRegistry().addTool(std::move(open));

    for (const char* name: {"file_new", "file_close"}) {
        ToolSpec t;
        t.name = name;
        const bool isNew = std::string(name) == "file_new";
        t.title = isNew ? "New document" : "Close document";
        t.description = isNew ? "Replaces the current document with a new, empty, untitled one. Unsaved changes: "
                                "see on_unsaved (default: fail and explain)." :
                                "Closes the current document; the app then shows a new, empty, untitled document "
                                "(it always has one open). Unsaved changes: see on_unsaved.";
        t.inputSchema = schema::object({{"on_unsaved", unsavedSchema()}});
        t.tier = Tier::Files;
        t.asyncHandler = [srv, ctrl, isNew](const json& j, Responder respond) {
            requireDocument(ctrl);
            Args args(j);
            args.rejectUnknown({"on_unsaved"});
            const std::string policy = args.choice("on_unsaved", {"fail", "save", "discard"}, "fail");
            handleUnsaved(
                    srv, ctrl, policy, isNew ? "creating a new document" : "closing the document",
                    [ctrl, respond]() {
                        ctrl->openFileWithoutSavingTheCurrentDocument({}, false, -1, [ctrl, respond](bool) {
                            respond(ToolResult::structured(fileInfo(ctrl), "New empty document."));
                        });
                    },
                    respond);
        };
        server.getRegistry().addTool(std::move(t));
    }

    ToolSpec save;
    save.name = "file_save";
    save.title = "Save";
    save.description = "Saves the current document to its file (.xopp). Untitled documents need file_save_as.";
    save.inputSchema = schema::object({});
    save.tier = Tier::Files;
    save.asyncHandler = [ctrl](const json&, Responder respond) {
        requireDocument(ctrl);
        fs::path path;
        {
            std::shared_lock lock(*ctrl->getDocument());
            path = ctrl->getDocument()->getFilepath();
        }
        if (path.empty()) {
            throw ToolError("The document is untitled; use file_save_as(path)");
        }
        if (path.extension() != ".xopp") {
            throw ToolError("The document was opened from " + toUtf8(path.filename()) +
                            "; use file_save_as with a .xopp path to save your notes");
        }
        saveTo(ctrl, respond, std::nullopt);
    };
    server.getRegistry().addTool(std::move(save));

    ToolSpec saveAs;
    saveAs.name = "file_save_as";
    saveAs.title = "Save as";
    saveAs.description =
            "Saves the current document under a new path (.xopp is added if missing) and continues working on "
            "that file. Overwriting a different existing file needs overwrite=true and the 'destructive' "
            "permission.";
    saveAs.inputSchema = schema::object(
            {{"path", schema::string("Target file")},
             {"overwrite", schema::withDefault(schema::boolean("Allow replacing an existing file"), false)}},
            {"path"});
    saveAs.tier = Tier::Files;
    saveAs.asyncHandler = [srv, ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"path", "overwrite"});
        fs::path path = absolutePath(args.str("path"));
        if (path.extension() != ".xopp") {
            path += ".xopp";
        }
        fs::path current;
        {
            std::shared_lock lock(*ctrl->getDocument());
            current = ctrl->getDocument()->getFilepath();
        }
        std::error_code ec;
        if (fs::exists(path, ec) && path != current) {
            if (!args.boolean("overwrite", false)) {
                throw ToolError(toUtf8(path) + " already exists; pass overwrite=true to replace it");
            }
            srv->requireTier(Tier::Destructive, "overwriting an existing file");
        }
        if (!fs::is_directory(path.parent_path(), ec)) {
            throw ToolError("Folder does not exist: " + toUtf8(path.parent_path()));
        }
        saveTo(ctrl, respond, path);
    };
    server.getRegistry().addTool(std::move(saveAs));

    ToolSpec recent;
    recent.name = "file_recent";
    recent.title = "Recent files";
    recent.description = "Recently used notes (.xopp/.xoj) and PDFs, most recent first.";
    recent.inputSchema = schema::object({});
    recent.readOnly = true;
    recent.idempotent = true;
    recent.handler = [](const json&) {
        auto files = RecentManager::getRecentFiles();
        auto list = [](const auto& infos) {
            json out = json::array();
            for (const auto& info: infos) {
                const char* uri = gtk_recent_info_get_uri(info.get());
                gchar* file = uri ? g_filename_from_uri(uri, nullptr, nullptr) : nullptr;
                if (file) {
                    out.push_back(file);
                    g_free(file);
                }
            }
            return out;
        };
        return ToolResult::structured({{"notes", list(files.recentXoppFiles)}, {"pdfs", list(files.recentPdfFiles)}});
    };
    server.getRegistry().addTool(std::move(recent));
}

}  // namespace xoj::mcp::tools
