/*
 * xournalai (based on Xournal++)
 *
 * Walking the application's menu model (menubar) for agents
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for shared_ptr
#include <string>  // for string
#include <vector>  // for vector

#include <gio/gio.h>  // for GMenuModel

namespace xoj::api {

struct MenuEntry {
    std::string path;  ///< "File/Recent Documents/notes.xopp" (labels without mnemonics)
    std::string label;
    std::string action;                ///< "win.open" (empty for submenus/sections)
    std::shared_ptr<GVariant> target;  ///< action target (parameter), may be null
    std::string accel;                 ///< e.g. "<Ctrl>o"
    bool submenu = false;
    int depth = 0;
};

/// Flattened menu tree in display order (submenus followed by their items)
std::vector<MenuEntry> walkMenu(GMenuModel* model);

/// Removes GTK mnemonic underscores ("_File" → "File", "__" → "_")
std::string stripMnemonic(const std::string& label);

}  // namespace xoj::api
