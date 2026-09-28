#include "Menus.h"

namespace xoj::api {

std::string stripMnemonic(const std::string& label) {
    std::string out;
    for (size_t i = 0; i < label.size(); i++) {
        if (label[i] == '_') {
            if (i + 1 < label.size() && label[i + 1] == '_') {
                out += '_';
                i++;
            }
            continue;
        }
        out += label[i];
    }
    return out;
}

namespace {

std::string attrString(GMenuModel* model, int i, const char* name) {
    gchar* value = nullptr;
    if (g_menu_model_get_item_attribute(model, i, name, "s", &value) && value) {
        std::string s = value;
        g_free(value);
        return s;
    }
    return {};
}

void walk(GMenuModel* model, const std::string& prefix, int depth, std::vector<MenuEntry>& out) {
    const int n = g_menu_model_get_n_items(model);
    for (int i = 0; i < n; i++) {
        const std::string label = stripMnemonic(attrString(model, i, G_MENU_ATTRIBUTE_LABEL));
        if (GMenuModel* section = g_menu_model_get_item_link(model, i, G_MENU_LINK_SECTION)) {
            walk(section, prefix, depth, out);  // sections are flat groups
            g_object_unref(section);
        }
        GMenuModel* sub = g_menu_model_get_item_link(model, i, G_MENU_LINK_SUBMENU);
        if (label.empty() && !sub) {
            continue;
        }
        MenuEntry e;
        e.label = label;
        e.path = prefix.empty() ? label : prefix + "/" + label;
        e.depth = depth;
        e.action = attrString(model, i, G_MENU_ATTRIBUTE_ACTION);
        e.accel = attrString(model, i, "accel");
        if (GVariant* t = g_menu_model_get_item_attribute_value(model, i, G_MENU_ATTRIBUTE_TARGET, nullptr)) {
            e.target = std::shared_ptr<GVariant>(t, g_variant_unref);
        }
        e.submenu = sub != nullptr;
        out.push_back(e);
        if (sub) {
            walk(sub, e.path, depth + 1, out);
            g_object_unref(sub);
        }
    }
}

}  // namespace

std::vector<MenuEntry> walkMenu(GMenuModel* model) {
    std::vector<MenuEntry> out;
    if (model) {
        walk(model, "", 0, out);
    }
    return out;
}

}  // namespace xoj::api
