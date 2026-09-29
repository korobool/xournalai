// Tools: create_text, create_latex, create_image, create_link

#include <fstream>   // for ifstream
#include <iterator>  // for istreambuf_iterator

#include "api/DrawApi.h"      // for DrawApi
#include "api/LatexApi.h"     // for typesetLatex
#include "control/Control.h"  // for Control
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "mcp/XJson.h"
#include "model/Image.h"     // for Image
#include "model/TexImage.h"  // for TexImage

#include "DrawCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::vector<schema::Property> withTarget(std::vector<schema::Property> props) {
    addTargetSchema(props);
    return props;
}

json fontSchema() {
    return schema::object({{"name", schema::string("Font family, e.g. \"Sans\", \"Serif\", \"Monospace\"")},
                           {"size", schema::number("Size in points")}},
                          {}, "Font (default Sans 12)");
}

}  // namespace

void registerContentTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec text;
    text.name = "create_text";
    text.title = "Create text";
    text.description = "Adds typed text boxes (multi-line with \\n) at x,y (top-left, page points). Optional font "
                       "{name,size}, color, align (left/center/right) and wrap width. Goes to the default layer (the "
                       "user's current layer unless they chose e.g. \"AI\"; see app_status).";
    text.inputSchema = schema::object(
            withTarget({{"texts",
                         schema::array("Text boxes",
                                       schema::object({{"text", schema::string("The text")},
                                                       {"x", schema::number("Left edge")},
                                                       {"y", schema::number("Top edge")},
                                                       {"font", fontSchema()},
                                                       {"color", schema::string("Text color")},
                                                       {"align",
                                                        schema::enumeration("Alignment", {"left", "center", "right"})},
                                                       {"wrap", schema::number("Wrap width in points")}},
                                                      {"text", "x", "y"}, "A text box"))}}),
            {"texts"});
    text.tier = Tier::Draw;
    text.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"texts", "page", "new_page", "layer", "animate", "speed"});
        const json& list = args.raw("texts");
        if (!list.is_array() || list.empty()) {
            throw ToolError("'texts' must be a non-empty array");
        }
        std::vector<std::string> warnings;
        std::vector<ElementPtr> elements;
        for (size_t i = 0; i < list.size(); i++) {
            json e = list[i];
            if (!e.is_object()) {
                throw ToolError("texts[" + std::to_string(i) + "] must be an object");
            }
            e["type"] = "text";
            if (!e.contains("color")) {
                e["color"] = colorToHex(defaultStyle(ctrl, false).color);
            }
            elements.push_back(xjson::elementFromXJson(e, "texts[" + std::to_string(i) + "]", warnings));
        }
        insertAndRespond(*srv, args, std::move(elements), respond);
    };
    server.getRegistry().addTool(std::move(text));

    ToolSpec link;
    link.name = "create_link";
    link.title = "Create link";
    link.description = "Adds clickable links (text + URL) at x,y (top-left, page points).";
    link.inputSchema = schema::object(
            withTarget({{"links", schema::array("Links", schema::object({{"text", schema::string("Link text")},
                                                                         {"url", schema::string("Target URL")},
                                                                         {"x", schema::number("Left edge")},
                                                                         {"y", schema::number("Top edge")},
                                                                         {"font", fontSchema()},
                                                                         {"color", schema::string("Color")}},
                                                                        {"text", "url", "x", "y"}, "A link"))}}),
            {"links"});
    link.tier = Tier::Draw;
    link.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"links", "page", "new_page", "layer", "animate", "speed"});
        const json& list = args.raw("links");
        if (!list.is_array() || list.empty()) {
            throw ToolError("'links' must be a non-empty array");
        }
        std::vector<std::string> warnings;
        std::vector<ElementPtr> elements;
        for (size_t i = 0; i < list.size(); i++) {
            json e = list[i];
            e["type"] = "link";
            elements.push_back(xjson::elementFromXJson(e, "links[" + std::to_string(i) + "]", warnings));
        }
        insertAndRespond(*srv, args, std::move(elements), respond);
    };
    server.getRegistry().addTool(std::move(link));

    ToolSpec image;
    image.name = "create_image";
    image.title = "Create image";
    image.description =
            "Inserts an image (PNG, JPEG, ...) from a file path or base64 data. Place it with x,y and optionally "
            "width and/or height (aspect ratio kept if only one is given); default size: the image's natural size "
            "at 72 dpi, shrunk to fit the page.";
    image.inputSchema = schema::object(withTarget({{"path", schema::string("Image file")},
                                                   {"data", schema::string("Base64 image data (or a data: URL)")},
                                                   {"x", schema::number("Left edge")},
                                                   {"y", schema::number("Top edge")},
                                                   {"width", schema::number("Width in points")},
                                                   {"height", schema::number("Height in points")}}),
                                       {"x", "y"});
    image.tier = Tier::Draw;
    image.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown(
                {"path", "data", "x", "y", "width", "height", "page", "new_page", "layer", "animate", "speed"});
        std::string bytes;
        if (args.has("path")) {
            srv->requireTier(Tier::Files, "reading an image file");
            std::ifstream in(pathFromUtf8(args.str("path")), std::ios::binary);
            if (!in) {
                throw ToolError("Cannot read " + args.str("path"));
            }
            bytes.assign(std::istreambuf_iterator<char>(in), {});
        } else if (args.has("data")) {
            bytes = base64Decode(args.str("data"));
        } else {
            throw ToolError("Give 'path' or 'data'");
        }
        auto img = std::make_unique<Image>();
        img->setImage(std::move(bytes));
        img->getBoundingBox();
        const auto nat = img->getNaturalSize();
        if (nat.width <= 0 || nat.height <= 0) {
            throw ToolError("The data is not an image format this app can read");
        }
        double w = args.number("width", 0), h = args.number("height", 0);
        if (w <= 0 && h <= 0) {
            w = nat.width;
            h = nat.height;
            const double maxW = 500;  // keep large images on the page
            if (w > maxW) {
                h *= maxW / w;
                w = maxW;
            }
        } else if (w <= 0) {
            w = h * nat.width / nat.height;
        } else if (h <= 0) {
            h = w * nat.height / nat.width;
        }
        img->setTransformation(xoj::util::Matrix::TRANSLATION(args.number("x"), args.number("y"))
                                       .scale(w / nat.width, h / nat.height));
        std::vector<ElementPtr> elements;
        elements.push_back(std::move(img));
        insertAndRespond(*srv, args, std::move(elements), respond);
    };
    server.getRegistry().addTool(std::move(image));

    ToolSpec latex;
    latex.name = "create_latex";
    latex.title = "Create LaTeX";
    latex.description =
            "Typesets a LaTeX formula with the user's LaTeX template (math mode, as in the app's LaTeX tool, "
            "e.g. \"\\\\int_0^1 x^2\\\\,dx\") and inserts it at x,y; 'height' scales it. Needs a TeX installation. "
            "Returns LaTeX errors if compilation fails.";
    latex.inputSchema = schema::object(withTarget({{"latex", schema::string("Formula (LaTeX source)")},
                                                   {"x", schema::number("Left edge")},
                                                   {"y", schema::number("Top edge")},
                                                   {"height", schema::number("Height in points (default: natural)")},
                                                   {"color", schema::string("Color")}}),
                                       {"latex", "x", "y"});
    latex.tier = Tier::Draw;
    latex.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"latex", "x", "y", "height", "color", "page", "new_page", "layer", "animate", "speed"});
        const std::string source = args.str("latex");
        if (source.find_first_not_of(" \t\n") == std::string::npos) {
            throw ToolError("'latex' is empty");
        }
        const double x = args.number("x"), y = args.number("y");
        const double height = args.number("height", 0, 0, 2000);
        const Color color = args.has("color") ? parseColor(args.raw("color")) : Color(0, 0, 0);
        if (!args.boolean("new_page", false)) {
            resolvePageIndex(ctrl, args);  // validate before the slow part
        }
        auto argsCopy = std::make_shared<json>(j);
        api::typesetLatex(ctrl, source, color, [srv, argsCopy, x, y, height, respond](api::LatexResult r) {
            if (!r.image) {
                respond(ToolResult::error(r.error));
                return;
            }
            try {
                auto nat = r.image->getNaturalSize();
                r.image->getBoundingBox();
                nat = r.image->getNaturalSize();
                const double s = height > 0 && nat.height > 0 ? height / nat.height : 1.0;
                r.image->setTransformation(xoj::util::Matrix::TRANSLATION(x, y).scale(s, s));
                std::vector<ElementPtr> elements;
                elements.push_back(std::move(r.image));
                insertAndRespond(*srv, Args(*argsCopy), std::move(elements), respond);
            } catch (const std::exception& e) {
                respond(ToolResult::error(e.what()));
            }
        });
    };
    server.getRegistry().addTool(std::move(latex));
}

}  // namespace xoj::mcp::tools
