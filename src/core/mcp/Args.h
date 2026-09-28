/*
 * Xournal++ (xournalai)
 *
 * Typed, forgiving access to tool arguments with agent-friendly error messages
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>           // for int64_t
#include <initializer_list>  // for initializer_list
#include <optional>          // for optional
#include <string>            // for string
#include <vector>            // for vector

#include "Json.h"

namespace xoj::mcp {

/**
 * @brief Reads tool arguments.
 *
 * Models from different vendors are not equally strict about JSON types, so numbers and booleans are also accepted
 * as strings ("3", "true"). Every problem throws a ToolError naming the argument, so the agent can fix the call.
 */
class Args {
public:
    explicit Args(const json& args);

    bool has(const std::string& key) const;
    const json& raw(const std::string& key) const;  ///< required, any type

    std::string str(const std::string& key) const;
    std::string str(const std::string& key, const std::string& def) const;
    int64_t integer(const std::string& key) const;
    int64_t integer(const std::string& key, int64_t def) const;
    int64_t integer(const std::string& key, int64_t def, int64_t min, int64_t max) const;
    double number(const std::string& key) const;
    double number(const std::string& key, double def) const;
    double number(const std::string& key, double def, double min, double max) const;
    bool boolean(const std::string& key, bool def) const;
    /// Value from a fixed set of strings
    std::string choice(const std::string& key, std::initializer_list<const char*> allowed,
                       const std::string& def) const;
    std::vector<double> numbers(const std::string& key) const;  ///< array of numbers (required)
    std::optional<std::vector<double>> numbersOpt(const std::string& key) const;

    /// Throws if the arguments contain keys that are not in `known` (catches typos such as "colour")
    void rejectUnknown(std::initializer_list<const char*> known) const;

    static double toNumber(const json& v, const std::string& what);
    static bool toBool(const json& v, const std::string& what);

private:
    const json& args;
};

}  // namespace xoj::mcp
