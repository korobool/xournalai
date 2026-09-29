/*
 * xournalai (based on Xournal++)
 *
 * Pen engine: an agent drives the application's real input pipeline with a simulated stylus
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>     // for size_t
#include <functional>  // for function
#include <optional>    // for optional
#include <string>      // for string
#include <vector>      // for vector

#include "control/ToolEnums.h"  // for ToolType, ToolSize
#include "util/Color.h"         // for Color

class Control;
class Element;

namespace xoj::api {

struct PenSample {
    double x = 0, y = 0;    ///< page points
    double pressure = 0.8;  ///< 0..1, like a hardware stylus
    double t = -1;          ///< ms since the stroke started (-1: derived from speed)
};

struct PenJob {
    size_t page = 0;
    ToolType tool = TOOL_PEN;
    std::optional<Color> color;
    std::optional<ToolSize> size;
    std::optional<DrawingType> drawingType;  ///< pen/highlighter mode: shapes, line, shape recognizer
    std::string layer = "current";           ///< layer to draw/erase/select in ("current", "AI", name, "#n")
    std::vector<std::vector<PenSample>> strokes;
    double speed = 1.0;            ///< 1 = hand speed; >1 faster; 0 = instant
    double pointsPerSecond = 400;  ///< hand speed when samples have no timestamps
};

struct PenResult {
    std::vector<const Element*> created;
    size_t erased = 0;
    size_t selected = 0;
    std::string layerName;
    size_t layer = 0;
    std::string error;  ///< set if the job could not run (e.g. the user kept drawing)
};

/**
 * @brief Replays pen strokes as synthetic stylus events through InputContext, exactly like hardware input: the
 * active tool, pressure settings, stabilizer, shape recognizer, eraser and selection tools all apply. The user's
 * tool (type, color, size) and selected layer are restored afterwards. Waits while the user is mid-stroke.
 * Must be called on the main thread; `done` is called when all strokes were replayed.
 */
void runPen(Control* control, PenJob job, std::function<void(PenResult)> done);

/// True while the user is in the middle of an input sequence (drawing, erasing, selecting) on any page
bool userInputActive(Control* control);

}  // namespace xoj::api
