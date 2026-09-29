/*
 * xournalai (based on Xournal++)
 *
 * Shared parts of the drawing tools: stroke style, pressure, target layer, insertion and results
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>    // for unique_ptr
#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "api/DrawApi.h"   // for DrawResult
#include "api/Pressure.h"  // for PressureProfile
#include "mcp/Args.h"
#include "mcp/Registry.h"
#include "mcp/Schema.h"
#include "model/Stroke.h"  // for Stroke, StrokeTool
#include "util/Color.h"    // for Color

class Control;

namespace xoj::mcp {
class McpServer;
}

namespace xoj::mcp::tools {

struct StrokeStyleSpec {
    Color color = Color(0, 0, 0);
    double width = 1.41;
    StrokeTool tool = StrokeTool::PEN;
    std::string lineStyle = "plain";
    std::optional<int> fill;                      ///< 0..255 alpha of the fill, closed shapes only
    std::optional<api::PressureProfile> profile;  ///< nullopt = constant width, no pressure
    double tremor = 0;                            ///< hand tremor amplitude in points
    double spacing = 1.0;                         ///< point spacing for resampling (0 = keep points)
};

/// Default style: the user's current pen color, medium width, "ink" pressure profile
StrokeStyleSpec defaultStyle(Control* ctrl, bool handDrawn);

/**
 * Reads style keys of `obj` on top of `base`: color, width, tool, line_style, fill_opacity, profile,
 * profile_options {base,min,taper_in,taper_out,variation,nib_angle,seed}, tremor, spacing.
 */
StrokeStyleSpec readStyle(const json& obj, StrokeStyleSpec base, const std::string& where);

/// Schema properties for the style keys (added to a tool's top level and to per-item objects)
void addStyleSchema(std::vector<schema::Property>& props);
/// Schema properties for page, layer, animate, speed
void addTargetSchema(std::vector<schema::Property>& props);

/**
 * Builds a stroke from a polyline. Explicit per-point `widths` (absolute) or `pressures` (0..1, mapped like the
 * user's stylus) win over the style's profile. Highlighter strokes never get pressure.
 */
std::unique_ptr<Stroke> buildStroke(std::vector<Point> points, const StrokeStyleSpec& style,
                                    const api::PressureSettings& pressure, const std::vector<double>* widths = nullptr,
                                    const std::vector<double>* pressures = nullptr,
                                    const std::vector<double>* times = nullptr);

/// Inserts elements according to page/layer/animate/speed arguments and responds with the created ids
void insertAndRespond(McpServer& server, const Args& args, std::vector<ElementPtr> elements, Responder respond,
                      json extra = json::object());

/**
 * @brief If args["profile"] == "match_user", replaces it with a pressure profile learned from the user's own recent
 * pen strokes and fills in their width and color (unless given). Returns the (possibly rewritten) arguments.
 */
json resolveMatchUser(Control* ctrl, const json& args);

/// The learned style as JSON (for the user_style tool); throws ToolError if the user has no pen strokes yet
json learnedUserStyle(Control* ctrl);

/// JSON summary of an insertion
json drawResultJson(const api::DrawResult& r);

}  // namespace xoj::mcp::tools
