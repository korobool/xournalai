// Tool: import

#include <cmath>     // for ceil
#include <fstream>   // for ifstream, ofstream
#include <iterator>  // for istreambuf_iterator

#include <cairo.h>
#include <poppler.h>
#include <unistd.h>  // for close

#include "control/Control.h"              // for Control
#include "control/xojfile/LoadHandler.h"  // for LoadHandler
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "mcp/XJson.h"
#include "model/Document.h"     // for Document
#include "model/Image.h"        // for Image
#include "model/Layer.h"        // for Layer
#include "model/PageType.h"     // for PageType
#include "model/XojPage.h"      // for XojPage
#include "util/ElementRange.h"  // for ElementRange

#include "DrawCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        throw ToolError("Cannot read " + toUtf8(p));
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

/// Writes inline data to a temporary file (for loaders that need a path)
fs::path tempFile(const std::string& data, const std::string& ext) {
    gchar* name = nullptr;
    const int fd = g_file_open_tmp(("xoai-import-XXXXXX." + ext).c_str(), &name, nullptr);
    if (fd < 0) {
        throw ToolError("Cannot create a temporary file");
    }
    close(fd);
    fs::path p(name);
    g_free(name);
    std::ofstream out(p, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return p;
}

json pagesResult(Control* ctrl, size_t first, size_t count, std::vector<std::string> warnings) {
    json pages = json::array();
    for (size_t i = 0; i < count; i++) {
        pages.push_back(first + i + 1);
    }
    json out = {{"inserted_pages", pages}, {"page_count", ctrl->getDocument()->getPageCount()}};
    if (!warnings.empty()) {
        out["warnings"] = warnings;
    }
    return out;
}

}  // namespace

void registerImportTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    std::vector<schema::Property> props = {
            {"kind", schema::enumeration("What is imported", {"svg", "image", "xjson", "xopp", "pdf"})},
            {"path", schema::string("Source file")},
            {"data", schema::string("Or inline content: SVG/xjson text, or base64 for image/xopp/pdf")},
            {"target", schema::array("svg/image: fit into [x, y, width, height]", schema::number("coordinate"))},
            {"x", schema::number("svg/image: left edge (without target)")},
            {"y", schema::number("svg/image: top edge (without target)")},
            {"dx", schema::number("xjson: move the imported elements by dx")},
            {"dy", schema::number("xjson: move the imported elements by dy")},
            {"pages", schema::string("xopp/pdf: which source pages, e.g. \"all\" (default), \"2\", \"1-3\"")},
            {"after_page", schema::integer("xopp/pdf: insert after this page (default: at the end)")},
            {"dpi", schema::withDefault(schema::number("pdf: rendering resolution"), 150)}};
    addTargetSchema(props);
    ToolSpec imp;
    imp.name = "import";
    imp.title = "Import";
    imp.description =
            "Imports external content. svg → editable strokes (like create_from_svg); image → image element; "
            "xjson → elements exactly as exported (move with dx/dy); xopp → pages copied from another Xournal++ "
            "file; pdf → pages of another PDF, each as a new page showing the PDF page as an image (a document "
            "can only have one background PDF; use file_open to annotate a PDF). Source: 'path' or inline 'data'.";
    imp.inputSchema = schema::object(std::move(props), {"kind"});
    imp.tier = Tier::Draw;
    imp.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"kind", "path", "data", "target", "x", "y", "dx", "dy", "pages", "after_page", "dpi",
                            "page", "new_page", "layer", "animate", "speed"});
        const std::string kind = args.choice("kind", {"svg", "image", "xjson", "xopp", "pdf"}, "");
        if (!args.has("path") && !args.has("data")) {
            throw ToolError("Give 'path' or 'data'");
        }
        if (args.has("path")) {
            srv->requireTier(Tier::Files, "reading a file to import");
        }

        if (kind == "svg" || kind == "image") {
            // Delegate to the creation tools with equivalent arguments
            json forwarded = j;
            forwarded.erase("kind");
            forwarded.erase("dx");
            forwarded.erase("dy");
            forwarded.erase("pages");
            forwarded.erase("after_page");
            forwarded.erase("dpi");
            if (kind == "svg" && forwarded.contains("data")) {
                forwarded["svg"] = forwarded["data"];
                forwarded.erase("data");
            }
            if (kind == "image" && forwarded.contains("target")) {
                const json t = forwarded["target"];
                forwarded.erase("target");
                if (t.is_array() && t.size() == 4) {
                    forwarded["x"] = t[0];
                    forwarded["y"] = t[1];
                    forwarded["width"] = t[2];
                    forwarded["height"] = t[3];
                }
            }
            if (kind == "image" && (!forwarded.contains("x") || !forwarded.contains("y"))) {
                forwarded["x"] = forwarded.value("x", json(40));
                forwarded["y"] = forwarded.value("y", json(40));
            }
            const ToolSpec* tool = srv->getRegistry().findTool(kind == "svg" ? "create_from_svg" : "create_image");
            tool->asyncHandler(forwarded, respond);
            return;
        }

        if (kind == "xjson") {
            json doc;
            if (args.has("data")) {
                const json& d = args.raw("data");
                doc = d.is_string() ? json::parse(d.get<std::string>(), nullptr, false) : d;
            } else {
                doc = json::parse(readAll(pathFromUtf8(args.str("path"))), nullptr, false);
            }
            if (doc.is_discarded()) {
                throw ToolError("The xjson data is not valid JSON");
            }
            std::vector<std::string> warnings;
            auto elements = xjson::parse(doc, warnings);
            if (elements.empty()) {
                throw ToolError("The xjson contains no elements");
            }
            const double dx = args.number("dx", 0), dy = args.number("dy", 0);
            if (dx != 0 || dy != 0) {
                for (auto& e: elements) {
                    e->move(dx, dy);
                }
            }
            json extra = json::object();
            if (!warnings.empty()) {
                extra["warnings"] = warnings;
            }
            insertAndRespond(*srv, args, std::move(elements), respond, extra);
            return;
        }

        // Page imports
        fs::path source;
        bool temporary = false;
        if (args.has("path")) {
            source = pathFromUtf8(args.str("path"));
        } else {
            source = tempFile(base64Decode(args.str("data")), kind);
            temporary = true;
        }
        Document* doc = ctrl->getDocument();
        size_t insertAt = doc->getPageCount();
        if (args.has("after_page")) {
            insertAt = static_cast<size_t>(args.integer("after_page", 0, 0, static_cast<int64_t>(insertAt)));
        }
        std::vector<std::string> warnings;
        std::vector<PageRef> newPages;

        if (kind == "xopp") {
            std::vector<std::string> errors;
            LoadHandler loader(&errors);
            auto other = loader.loadDocument(source);
            if (temporary) {
                fs::remove(source);
            }
            if (!other) {
                throw ToolError("Could not load the .xopp: " +
                                (errors.empty() ? std::string("unknown error") : errors[0]));
            }
            for (size_t i = 0; i < other->getPageCount(); i++) {
                PageRef p(other->getPage(i)->clone());
                if (p->getBackgroundType().isPdfPage()) {
                    p->setBackgroundType(PageType(PageTypeFormat::Plain));
                    warnings.push_back("Page " + std::to_string(i + 1) +
                                       " had a PDF background, which cannot be imported; it is plain now");
                }
                newPages.push_back(p);
            }
        } else {  // pdf
            GError* err = nullptr;
            gchar* uri = g_filename_to_uri(source.c_str(), nullptr, nullptr);
            PopplerDocument* pdf = poppler_document_new_from_file(uri, nullptr, &err);
            g_free(uri);
            if (!pdf) {
                std::string msg = err ? err->message : "unknown error";
                if (err) {
                    g_error_free(err);
                }
                if (temporary) {
                    fs::remove(source);
                }
                throw ToolError("Could not open the PDF: " + msg);
            }
            const double scale = args.number("dpi", 150, 30, 600) / 72.0;
            const int n = poppler_document_get_n_pages(pdf);
            for (int i = 0; i < n && i < 200; i++) {
                PopplerPage* pp = poppler_document_get_page(pdf, i);
                double w = 0, h = 0;
                poppler_page_get_size(pp, &w, &h);
                cairo_surface_t* s =
                        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(w * scale)),
                                                   static_cast<int>(std::ceil(h * scale)));
                cairo_t* cr = cairo_create(s);
                cairo_set_source_rgb(cr, 1, 1, 1);
                cairo_paint(cr);
                cairo_scale(cr, scale, scale);
                poppler_page_render(pp, cr);
                cairo_destroy(cr);
                auto img = std::make_unique<Image>();
                img->setImage(encodePng(s));
                cairo_surface_destroy(s);
                g_object_unref(pp);
                img->getBoundingBox();
                const auto nat = img->getNaturalSize();
                img->setTransformation(xoj::util::Matrix::SCALING(w / nat.width, h / nat.height));
                PageRef page = std::make_shared<XojPage>(w, h);
                page->setBackgroundType(PageType(PageTypeFormat::Plain));
                page->getLayers()[0]->addElement(std::move(img));
                newPages.push_back(page);
            }
            if (n > 200) {
                warnings.push_back("Only the first 200 pages were imported");
            }
            g_object_unref(pdf);
            if (temporary) {
                fs::remove(source);
            }
        }

        // Page selection
        const std::string spec = args.str("pages", "all");
        std::vector<PageRef> chosen;
        if (spec == "all") {
            chosen = newPages;
        } else {
            PageRangeVector ranges;
            try {
                ranges = ElementRange::parse(spec, newPages.size());
            } catch (const std::exception& e) {
                throw ToolError("Invalid pages '" + spec + "': " + e.what());
            }
            for (const auto& r: ranges) {
                for (size_t i = r.first; i <= r.last && i < newPages.size(); i++) {
                    chosen.push_back(newPages[i]);
                }
            }
        }
        if (chosen.empty()) {
            throw ToolError("No pages to import");
        }
        for (size_t i = 0; i < chosen.size(); i++) {
            ctrl->insertPage(chosen[i], insertAt + i, false);
        }
        respond(ToolResult::structured(pagesResult(ctrl, insertAt, chosen.size(), warnings)));
    };
    server.getRegistry().addTool(std::move(imp));
}

}  // namespace xoj::mcp::tools
