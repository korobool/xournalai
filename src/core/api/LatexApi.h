/*
 * xournalai (based on Xournal++)
 *
 * Typesetting LaTeX formulas into TexImage elements without the LaTeX dialog
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <memory>      // for unique_ptr
#include <string>      // for string

#include "util/Color.h"  // for Color

class Control;
class TexImage;

namespace xoj::api {

struct LatexResult {
    std::unique_ptr<TexImage> image;  ///< null on failure
    std::string error;                ///< message and LaTeX log excerpt on failure
};

/**
 * @brief Compiles `latex` (the formula body, as typed in the LaTeX tool) with the user's LaTeX template and
 * settings, asynchronously on the main loop, and calls `done` with the result. Each call uses its own
 * temporary folder, so several typesetting jobs may run at once.
 */
void typesetLatex(Control* control, const std::string& latex, Color color, std::function<void(LatexResult)> done);

}  // namespace xoj::api
