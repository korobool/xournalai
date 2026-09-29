/*
 * xournalai (based on Xournal++)
 *
 * GVariant <-> JSON conversion for action states and parameters
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <glib.h>  // for GVariant

#include "Json.h"

namespace xoj::mcp {

/// Converts basic GVariants (bool, integers, double, string, tuples, arrays) to JSON; others become their text form
json variantToJson(GVariant* v);

/**
 * @brief Builds a GVariant of `type` from JSON. Basic types are converted directly; any type can also be given as
 * GVariant text (e.g. "(1, 'a')"). Returns a floating reference; throws ToolError if the value does not fit.
 */
GVariant* jsonToVariant(const json& value, const GVariantType* type);

}  // namespace xoj::mcp
