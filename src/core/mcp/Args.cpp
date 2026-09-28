#include "Args.h"

#include <cmath>    // for isfinite, floor
#include <cstdlib>  // for strtod

#include "Registry.h"  // for ToolError

namespace xoj::mcp {

Args::Args(const json& args): args(args) {
    if (!args.is_object()) {
        throw ToolError("Arguments must be a JSON object");
    }
}

bool Args::has(const std::string& key) const { return args.contains(key) && !args[key].is_null(); }

const json& Args::raw(const std::string& key) const {
    if (!has(key)) {
        throw ToolError("Missing required argument '" + key + "'");
    }
    return args[key];
}

double Args::toNumber(const json& v, const std::string& what) {
    if (v.is_number()) {
        return v.get<double>();
    }
    if (v.is_string()) {
        const auto& s = v.get_ref<const std::string&>();
        char* end = nullptr;
        const double d = std::strtod(s.c_str(), &end);
        if (!s.empty() && end && *end == '\0' && std::isfinite(d)) {
            return d;
        }
    }
    throw ToolError("Argument '" + what + "' must be a number, got " + v.dump());
}

bool Args::toBool(const json& v, const std::string& what) {
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    if (v.is_string()) {
        const auto& s = v.get_ref<const std::string&>();
        if (s == "true" || s == "1" || s == "yes") {
            return true;
        }
        if (s == "false" || s == "0" || s == "no") {
            return false;
        }
    }
    if (v.is_number_integer()) {
        return v.get<int64_t>() != 0;
    }
    throw ToolError("Argument '" + what + "' must be true or false, got " + v.dump());
}

std::string Args::str(const std::string& key) const {
    const json& v = raw(key);
    if (v.is_string()) {
        return v.get<std::string>();
    }
    if (v.is_number() || v.is_boolean()) {
        return v.dump();
    }
    throw ToolError("Argument '" + key + "' must be a string, got " + v.dump());
}

std::string Args::str(const std::string& key, const std::string& def) const { return has(key) ? str(key) : def; }

int64_t Args::integer(const std::string& key) const {
    const double d = toNumber(raw(key), key);
    if (std::floor(d) != d) {
        throw ToolError("Argument '" + key + "' must be a whole number, got " + raw(key).dump());
    }
    return static_cast<int64_t>(d);
}

int64_t Args::integer(const std::string& key, int64_t def) const { return has(key) ? integer(key) : def; }

int64_t Args::integer(const std::string& key, int64_t def, int64_t min, int64_t max) const {
    const int64_t v = integer(key, def);
    if (v < min || v > max) {
        throw ToolError("Argument '" + key + "' must be between " + std::to_string(min) + " and " +
                        std::to_string(max) + ", got " + std::to_string(v));
    }
    return v;
}

double Args::number(const std::string& key) const { return toNumber(raw(key), key); }

double Args::number(const std::string& key, double def) const { return has(key) ? number(key) : def; }

double Args::number(const std::string& key, double def, double min, double max) const {
    const double v = number(key, def);
    if (v < min || v > max) {
        throw ToolError("Argument '" + key + "' must be between " + json(min).dump() + " and " + json(max).dump() +
                        ", got " + json(v).dump());
    }
    return v;
}

bool Args::boolean(const std::string& key, bool def) const { return has(key) ? toBool(args[key], key) : def; }

std::string Args::choice(const std::string& key, std::initializer_list<const char*> allowed,
                         const std::string& def) const {
    if (!has(key)) {
        return def;
    }
    const std::string v = str(key);
    std::string list;
    for (const char* a: allowed) {
        if (v == a) {
            return v;
        }
        list += list.empty() ? "" : ", ";
        list += a;
    }
    throw ToolError("Argument '" + key + "' must be one of: " + list + " (got '" + v + "')");
}

std::vector<double> Args::numbers(const std::string& key) const {
    const json& v = raw(key);
    if (!v.is_array()) {
        throw ToolError("Argument '" + key + "' must be an array of numbers");
    }
    std::vector<double> out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        out.push_back(toNumber(v[i], key + "[" + std::to_string(i) + "]"));
    }
    return out;
}

std::optional<std::vector<double>> Args::numbersOpt(const std::string& key) const {
    if (!has(key)) {
        return std::nullopt;
    }
    return numbers(key);
}

void Args::rejectUnknown(std::initializer_list<const char*> known) const {
    for (const auto& [key, value]: args.items()) {
        bool ok = false;
        for (const char* k: known) {
            ok = ok || key == k;
        }
        if (!ok) {
            std::string list;
            for (const char* k: known) {
                list += list.empty() ? "" : ", ";
                list += k;
            }
            throw ToolError("Unknown argument '" + key + "'. Valid arguments: " + list);
        }
    }
}

}  // namespace xoj::mcp
