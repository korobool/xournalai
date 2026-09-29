#include "NotesStore.h"

#include <algorithm>     // for find_if
#include <cmath>         // for abs
#include <fstream>       // for ifstream, ofstream
#include <shared_mutex>  // for shared_lock

#include <glib.h>  // for g_warning

#include "control/Control.h"  // for Control
#include "model/Document.h"   // for Document
#include "util/Util.h"        // for npos

#include "Json.h"
#include "PathText.h"

namespace xoj::mcp {

namespace {
bool sameRegion(const std::optional<xoj::util::Rectangle<double>>& a,
                const std::optional<xoj::util::Rectangle<double>>& b) {
    if (!a || !b) {
        return !a && !b;
    }
    auto near = [](double x, double y) { return std::abs(x - y) < 0.5; };
    return near(a->x, b->x) && near(a->y, b->y) && near(a->width, b->width) && near(a->height, b->height);
}
}  // namespace

NotesStore::NotesStore(Control* control): control(control) {
    registerListener(control);
    documentPath = control->getDocument()->getFilepath();
    load();
}

NotesStore::~NotesStore() { unregisterListener(); }

fs::path NotesStore::sidecarFor(const fs::path& document) {
    fs::path p = document;
    p += ".ai-notes.json";
    return p;
}

fs::path NotesStore::file() const { return documentPath.empty() ? fs::path() : sidecarFor(documentPath); }

void NotesStore::documentChanged(DocumentChangeType type) {
    if (type == DOCUMENT_CHANGE_COMPLETE || type == DOCUMENT_CHANGE_CLEARED) {
        replaced = true;
    }
}

void NotesStore::sync() {
    const fs::path current = control->getDocument()->getFilepath();
    if (replaced) {
        // Another document (opened or new): its own notes. Every change was written already.
        replaced = false;
        notes.clear();
        documentPath = current;
        load();
    } else if (current != documentPath) {
        // Saved under a (new) name: the notes move with the document
        documentPath = current;
        save();
    }
}

std::vector<Note> NotesStore::all() const {
    // Notes of deleted pages stay in memory (undo brings the page back) but are not listed or saved
    Document* doc = control->getDocument();
    std::shared_lock lock(*doc);
    std::vector<Note> out;
    for (const auto& n: notes) {
        if (!n.page || doc->indexOf(n.page) != npos) {
            out.push_back(n);
        }
    }
    return out;
}

bool NotesStore::set(Note note) {
    if (note.text.size() > MAX_TEXT) {
        throw std::invalid_argument("Note text is too long (at most " + std::to_string(MAX_TEXT) + " bytes)");
    }
    auto it = std::find_if(notes.begin(), notes.end(), [&](const Note& n) {
        return n.page == note.page && n.kind == note.kind && sameRegion(n.region, note.region);
    });
    bool changed = false;
    if (note.text.empty()) {
        if (it != notes.end()) {
            notes.erase(it);
            changed = true;
        }
    } else if (it != notes.end()) {
        changed = it->text != note.text;
        *it = std::move(note);
    } else {
        if (notes.size() >= MAX_NOTES) {
            throw std::invalid_argument("Too many notes (at most " + std::to_string(MAX_NOTES) +
                                        "); delete or merge some");
        }
        notes.push_back(std::move(note));
        changed = true;
    }
    if (changed) {
        save();
    }
    return changed;
}

void NotesStore::load() {
    const fs::path path = file();
    if (path.empty() || !fs::exists(path)) {
        return;
    }
    try {
        std::ifstream in(path);
        json j = json::parse(in);
        Document* doc = control->getDocument();
        std::shared_lock lock(*doc);
        for (const auto& n: j.value("notes", json::array())) {
            Note note;
            const int page = n.value("page", 0);
            if (page > 0) {
                if (static_cast<size_t>(page) > doc->getPageCount()) {
                    continue;
                }
                note.page = doc->getPage(static_cast<size_t>(page - 1));
            }
            if (n.contains("region") && n["region"].is_array() && n["region"].size() == 4) {
                const auto& r = n["region"];
                note.region = xoj::util::Rectangle<double>(r[0].get<double>(), r[1].get<double>(), r[2].get<double>(),
                                                           r[3].get<double>());
            }
            note.kind = n.value("kind", "note");
            note.text = n.value("text", "");
            note.updated = n.value("updated", "");
            if (!note.text.empty() && notes.size() < MAX_NOTES) {
                notes.push_back(std::move(note));
            }
        }
    } catch (const std::exception& e) {
        g_warning("Could not read AI notes %s: %s", toUtf8(path).c_str(), e.what());
    }
}

void NotesStore::save() const {
    const fs::path path = file();
    if (path.empty()) {
        return;  // kept in memory until the document has a file name
    }
    json list = json::array();
    {
        Document* doc = control->getDocument();
        std::shared_lock lock(*doc);
        for (const auto& n: notes) {
            json j = {{"kind", n.kind}, {"text", n.text}, {"updated", n.updated}};
            if (n.page) {
                const size_t index = doc->indexOf(n.page);
                if (index == npos) {
                    continue;
                }
                j["page"] = index + 1;
            }
            if (n.region) {
                j["region"] = {n.region->x, n.region->y, n.region->width, n.region->height};
            }
            list.push_back(std::move(j));
        }
    }
    try {
        if (list.empty()) {
            if (fs::exists(path)) {
                fs::remove(path);
            }
            return;
        }
        const fs::path tmp = fs::path(path).concat(".tmp");
        {
            std::ofstream out(tmp);
            out << json({{"format", "xournalai-notes"}, {"version", 1}, {"notes", list}}).dump(1) << "\n";
        }
        fs::rename(tmp, path);
    } catch (const std::exception& e) {
        g_warning("Could not write AI notes %s: %s", toUtf8(path).c_str(), e.what());
    }
}

}  // namespace xoj::mcp
