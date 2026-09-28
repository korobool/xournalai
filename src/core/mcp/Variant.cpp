#include "Variant.h"

#include <cmath>  // for floor

#include "Args.h"
#include "Registry.h"  // for ToolError

namespace xoj::mcp {

json variantToJson(GVariant* v) {
    if (!v) {
        return nullptr;
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN)) {
        return static_cast<bool>(g_variant_get_boolean(v));
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT32)) {
        return g_variant_get_int32(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_UINT32)) {
        return g_variant_get_uint32(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT64)) {
        return g_variant_get_int64(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_UINT64)) {
        return g_variant_get_uint64(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_BYTE)) {
        return g_variant_get_byte(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_DOUBLE)) {
        return g_variant_get_double(v);
    }
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_STRING)) {
        return g_variant_get_string(v, nullptr);
    }
    if (g_variant_is_container(v) && !g_variant_is_of_type(v, G_VARIANT_TYPE_VARIANT)) {
        json arr = json::array();
        const gsize n = g_variant_n_children(v);
        for (gsize i = 0; i < n; i++) {
            GVariant* c = g_variant_get_child_value(v, i);
            arr.push_back(variantToJson(c));
            g_variant_unref(c);
        }
        return arr;
    }
    gchar* text = g_variant_print(v, FALSE);
    json out = text;
    g_free(text);
    return out;
}

GVariant* jsonToVariant(const json& value, const GVariantType* type) {
    const std::string t(g_variant_type_peek_string(type), g_variant_type_get_string_length(type));
    auto integer = [&](double min, double max) {
        const double d = Args::toNumber(value, "value");
        if (std::floor(d) != d || d < min || d > max) {
            throw ToolError("Value " + value.dump() + " is not a valid integer for type '" + t + "'");
        }
        return d;
    };
    if (t == "b") {
        return g_variant_new_boolean(Args::toBool(value, "value"));
    }
    if (t == "i") {
        return g_variant_new_int32(static_cast<gint32>(integer(-2147483648.0, 2147483647.0)));
    }
    if (t == "u") {
        return g_variant_new_uint32(static_cast<guint32>(integer(0, 4294967295.0)));
    }
    if (t == "x") {
        return g_variant_new_int64(static_cast<gint64>(integer(-9.2e18, 9.2e18)));
    }
    if (t == "t") {
        return g_variant_new_uint64(static_cast<guint64>(integer(0, 1.8e19)));
    }
    if (t == "y") {
        return g_variant_new_byte(static_cast<guint8>(integer(0, 255)));
    }
    if (t == "d") {
        return g_variant_new_double(Args::toNumber(value, "value"));
    }
    if (t == "s" && value.is_string()) {
        return g_variant_new_string(value.get<std::string>().c_str());
    }
    // Anything else: GVariant text format
    const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
    GError* err = nullptr;
    GVariant* v = g_variant_parse(type, text.c_str(), nullptr, nullptr, &err);
    if (!v) {
        std::string msg = err ? err->message : "parse error";
        if (err) {
            g_error_free(err);
        }
        throw ToolError("Value " + value.dump() + " does not fit type '" + t + "': " + msg);
    }
    return v;
}

}  // namespace xoj::mcp
