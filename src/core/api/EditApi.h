/*
 * xournalai (based on Xournal++)
 *
 * Editing existing elements with proper undo: move, scale, rotate, restyle, reorder, change layer, delete, select
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/Element.h"    // for Element
#include "model/LineStyle.h"  // for LineStyle
#include "model/PageRef.h"    // for PageRef
#include "util/Color.h"       // for Color

class Control;
class Layer;

namespace xoj::api {

/// Elements resolved from ids, all on one page, grouped by layer
struct ElementGroup {
    PageRef page;
    size_t pageIndex = 0;
    std::vector<std::pair<Layer*, std::vector<Element*>>> byLayer;
    std::vector<Element*> all;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;  ///< combined bounding box
};

struct Restyle {
    std::optional<Color> color;
    std::optional<double> width;  ///< strokes; per-point widths are scaled along
    std::optional<int> fill;      ///< strokes: -1 = no fill, 0..255 fill alpha
    std::optional<LineStyle> lineStyle;
};

/// All operations are one undo step each and must run on the main thread. Throws std::invalid_argument on bad input.
class EditApi {
public:
    explicit EditApi(Control* control): control(control) {}

    /// Resolves ids (ends the user's current selection first, so selected elements are back in their layers)
    ElementGroup resolve(const std::vector<std::string>& ids);

    void move(const ElementGroup& g, double dx, double dy);
    /// Scale about (x0, y0); keepLineWidth keeps stroke widths unchanged
    void scale(const ElementGroup& g, double fx, double fy, double x0, double y0, bool keepLineWidth);
    /// Rotate by `degrees` clockwise (on the page) about (x0, y0)
    void rotate(const ElementGroup& g, double degrees, double x0, double y0);
    /// Returns how many elements changed
    size_t restyle(const ElementGroup& g, const Restyle& style);
    /// "front" or "back" within each layer
    void reorder(const ElementGroup& g, const std::string& where);
    /// Moves the elements into another layer of the same page (layer resolved like DrawApi: name, "#n", "current")
    std::string toLayer(const ElementGroup& g, const std::string& layer);
    /// Deletes the elements (undoable; their ids come back with undo)
    size_t remove(const ElementGroup& g);
    /// Shows the elements as the user's selection (switches to their layer); all must be on one layer
    void select(const ElementGroup& g);

private:
    Control* control;
};

}  // namespace xoj::api
