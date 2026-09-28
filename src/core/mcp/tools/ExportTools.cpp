// Tool: export

#include <fstream>       // for ifstream
#include <shared_mutex>  // for shared_lock
#include <sstream>       // for stringstream

#include "api/DocumentApi.h"              // for elementsOnPage, locateId
#include "api/RenderApi.h"                // for renderPage, renderSvg
#include "control/Control.h"              // for Control
#include "control/ExportHelper.h"         // for exportPdf
#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "mcp/XJson.h"
#include "model/Document.h"     // for Document
#include "util/ElementRange.h"  // for ElementRange

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

constexpr size_t MAX_INLINE_BYTES = 5 * 1024 * 1024;

/// Parses "all", "current", "3" or "1-3,5" into 0-based page indices
std::vector<size_t> parsePages(Control* ctrl, const std::string& spec) {
    const size_t count = ctrl->getDocument()->getPageCount();
    std::vector<size_t> pages;
    if (spec == "all") {
        for (size_t i = 0; i < count; i++) {
            pages.push_back(i);
        }
        return pages;
    }
    if (spec == "current") {
        return {ctrl->getCurrentPageNo()};
    }
    PageRangeVector ranges;
    try {
        ranges = ElementRange::parse(spec, count);
    } catch (const std::exception& e) {
        throw ToolError("Invalid pages '" + spec + "' (use \"all\", \"current\", \"3\" or \"1-3,5\"): " + e.what());
    }
    for (const auto& r: ranges) {
        for (size_t i = r.first; i <= r.last && i < count; i++) {
            pages.push_back(i);
        }
    }
    if (pages.empty()) {
        throw ToolError("Pages '" + spec + "' select nothing; the document has " + std::to_string(count) + " page(s)");
    }
    return pages;
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string docStem(Control* ctrl) {
    std::shared_lock lock(*ctrl->getDocument());
    auto path = ctrl->getDocument()->getFilepath();
    return path.empty() ? "untitled" : toUtf8(path.stem());
}

}  // namespace

void registerExportTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec exp;
    exp.name = "export";
    exp.title = "Export";
    exp.description =
            "Exports content. Formats: pdf (pages, vector, with PDF backgrounds), png (raster, per page, "
            "region/dpi), svg (vector, per page, region), xopp (a copy of the whole document), xjson (lossless "
            "JSON of elements, importable again - see import). Select content with pages (\"all\", \"current\", "
            "\"2\", \"1-3,5\"), region (single page), layers, or element_ids (xjson). Output goes to 'path' (a "
            "file; for several png/svg pages '-p<N>' is appended) or to the export folder; delivery=inline "
            "returns the content in the reply instead (max 5 MB).";
    exp.inputSchema = schema::object(
            {{"format", schema::enumeration("Output format", {"pdf", "png", "svg", "xopp", "xjson"})},
             {"pages", schema::string("Pages: \"all\", \"current\", \"2\" or \"1-3,5\". Default: all for pdf/xopp, "
                                      "current otherwise")},
             {"region", schema::array("png/svg/xjson: only this area [x, y, width, height] (single page)",
                                      schema::number("coordinate"))},
             {"layers", schema::array("Only these layers (1-based)", schema::integer("layer"))},
             {"element_ids", schema::array("xjson: exactly these elements", schema::string("element id"))},
             {"background", schema::withDefault(schema::boolean("Include paper/ruling/PDF backgrounds"), true)},
             {"dpi", schema::withDefault(schema::number("png resolution"), 150)},
             {"path", schema::string("Output file (default: export folder)")},
             {"overwrite", schema::withDefault(schema::boolean("Replace an existing file"), false)},
             {"delivery",
              schema::withDefault(schema::enumeration("Where the result goes", {"path", "inline"}), "path")}},
            {"format"});
    exp.tier = Tier::Files;
    exp.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"format", "pages", "region", "layers", "element_ids", "background", "dpi", "path",
                            "overwrite", "delivery"});
        const std::string format = args.choice("format", {"pdf", "png", "svg", "xopp", "xjson"}, "");
        const bool inlineResult = args.choice("delivery", {"path", "inline"}, "path") == "inline";
        const bool background = args.boolean("background", true);
        const auto region = parseRegion(args);
        Document* doc = ctrl->getDocument();

        std::vector<size_t> layers;
        if (args.has("layers")) {
            for (double l: args.numbers("layers")) {
                layers.push_back(static_cast<size_t>(l));
            }
        }
        std::vector<size_t> pages;
        {
            std::shared_lock lock(*doc);
            pages = parsePages(ctrl, args.str("pages", format == "pdf" || format == "xopp" ? "all" : "current"));
        }
        if (region && pages.size() != 1) {
            throw ToolError("'region' needs exactly one page");
        }
        if (region && (format == "pdf" || format == "xopp")) {
            throw ToolError("'region' is not supported for " + format + "; use svg or png");
        }

        // Output paths
        const std::string stem = docStem(ctrl);
        auto outputPath = [&](const std::string& ext, std::optional<size_t> page) -> fs::path {
            fs::path base;
            if (args.has("path")) {
                base = fs::absolute(pathFromUtf8(args.str("path")));
                if (base.extension() == "." + ext) {
                    base.replace_extension();
                }
            } else {
                GDateTime* now = g_date_time_new_now_local();
                gchar* stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
                g_date_time_unref(now);
                base = srv->getConfig().exportDir / pathFromUtf8(stem + "-" + stamp);
                g_free(stamp);
            }
            if (page) {
                base += "-p" + std::to_string(*page + 1);
            }
            base += "." + ext;
            return base;
        };
        auto checkWritable = [&](const fs::path& p) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
            if (!fs::is_directory(p.parent_path(), ec)) {
                throw ToolError("Cannot create folder " + toUtf8(p.parent_path()));
            }
            if (fs::exists(p, ec)) {
                if (!args.boolean("overwrite", false)) {
                    throw ToolError(toUtf8(p) + " exists; pass overwrite=true to replace it");
                }
                srv->requireTier(Tier::Destructive, "overwriting an existing file");
            }
        };

        json files = json::array();
        ToolResult result;
        json summary = {{"format", format}, {"pages", json::array()}};
        for (size_t p: pages) {
            summary["pages"].push_back(p + 1);
        }

        if (format == "png" || format == "svg") {
            std::vector<std::pair<size_t, std::string>> outputs;
            size_t total = 0;
            for (size_t p: pages) {
                api::RenderOptions o;
                o.page = p;
                o.region = region;
                o.background = background;
                o.dpi = args.number("dpi", 150, 10, 1200);
                o.maxPixels = 16000;
                if (!layers.empty()) {
                    o.layers = layers;
                }
                std::string data = format == "png" ? api::renderPage(doc, o).png : api::renderSvg(doc, o);
                total += data.size();
                outputs.emplace_back(p, std::move(data));
            }
            if (inlineResult) {
                if (total > MAX_INLINE_BYTES) {
                    throw ToolError("Result too large for inline delivery (" + std::to_string(total / 1024) +
                                    " KB); use delivery=path");
                }
                result = ToolResult::structured(summary);
                for (auto& [p, data]: outputs) {
                    if (format == "png") {
                        result.addImage(base64Encode(data), "image/png");
                    } else {
                        result.addText("SVG of page " + std::to_string(p + 1) + ":\n" + data);
                    }
                }
                return result;
            }
            for (auto& [p, data]: outputs) {
                const fs::path file = outputPath(format, pages.size() > 1 ? std::optional<size_t>(p) : std::nullopt);
                checkWritable(file);
                std::ofstream out(file, std::ios::binary);
                out.write(data.data(), static_cast<std::streamsize>(data.size()));
                if (!out) {
                    throw ToolError("Could not write " + toUtf8(file));
                }
                files.push_back(toUtf8(file));
            }
        } else if (format == "xjson") {
            std::vector<const Element*> elements;
            {
                std::shared_lock lock(*doc);
                if (args.has("element_ids")) {
                    for (const auto& id: args.raw("element_ids")) {
                        elements.push_back(api::locateId(doc, id.get<std::string>()).element);
                    }
                } else {
                    for (size_t p: pages) {
                        for (const auto& loc: api::elementsOnPage(doc->getPage(p), p, std::nullopt, region)) {
                            if (layers.empty() || std::find(layers.begin(), layers.end(), loc.layer) != layers.end()) {
                                elements.push_back(loc.element);
                            }
                        }
                    }
                }
            }
            json meta = {{"app", "xournalai"}, {"document", stem}, {"pages", summary["pages"]}};
            if (region) {
                meta["region"] = {region->x, region->y, region->width, region->height};
            }
            json xj = xjson::serialize(elements, meta);
            const std::string text = xj.dump();
            if (inlineResult) {
                if (text.size() > MAX_INLINE_BYTES) {
                    throw ToolError("Result too large for inline delivery; use delivery=path");
                }
                return ToolResult::structured(std::move(xj), std::to_string(elements.size()) + " element(s) as xjson:");
            }
            const fs::path file = outputPath("xjson", std::nullopt);
            checkWritable(file);
            std::ofstream out(file);
            out << text;
            files.push_back(toUtf8(file));
            summary["elements"] = elements.size();
        } else {  // pdf, xopp: written by the application's own exporters
            const fs::path file =
                    inlineResult ? fs::path(g_get_tmp_dir()) / pathFromUtf8("xoai-export-" + stem + "." + format) :
                                   outputPath(format, std::nullopt);
            if (!inlineResult) {
                checkWritable(file);
            }
            if (format == "pdf") {
                std::string range;
                for (size_t p: pages) {
                    range += (range.empty() ? "" : ",") + std::to_string(p + 1);
                }
                std::string layerRange;
                for (size_t l: layers) {
                    layerRange += (layerRange.empty() ? "" : ",") + std::to_string(l);
                }
                try {
                    ExportHelper::exportPdf(doc, file, range.c_str(), layers.empty() ? nullptr : layerRange.c_str(),
                                            background ? EXPORT_BACKGROUND_ALL : EXPORT_BACKGROUND_NONE, false);
                } catch (const std::exception& e) {
                    throw ToolError(std::string("PDF export failed: ") + e.what());
                }
            } else {
                if (pages.size() != doc->getPageCount()) {
                    throw ToolError("xopp export copies the whole document; omit 'pages' (use xjson for parts)");
                }
                SaveHandler saver;
                {
                    std::shared_lock lock(*doc);
                    saver.prepareSave(doc, file);
                }
                saver.saveTo(file);
                if (!saver.getErrorMessage().empty()) {
                    throw ToolError("xopp export failed: " + saver.getErrorMessage());
                }
            }
            if (inlineResult) {
                std::string data = readFile(file);
                fs::remove(file);
                if (data.size() > MAX_INLINE_BYTES) {
                    throw ToolError("Result too large for inline delivery; use delivery=path");
                }
                result = ToolResult::structured(summary);
                result.addText("Base64 " + format + " (" + std::to_string(data.size()) + " bytes):");
                result.addText(base64Encode(data));
                return result;
            }
            files.push_back(toUtf8(file));
        }
        summary["files"] = files;
        return ToolResult::structured(summary, "Exported " + std::to_string(files.size()) + " file(s).");
    };
    server.getRegistry().addTool(std::move(exp));
}

}  // namespace xoj::mcp::tools
