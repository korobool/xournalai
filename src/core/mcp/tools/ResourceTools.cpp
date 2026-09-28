// Resources: xournal://document, xournal://changes, xournal://page/{page}, xournal://page/{page}/image
// Subscriptions: notifications/resources/updated on every document change

#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"  // for elementsOnPage
#include "api/EventHub.h"     // for EventHub
#include "api/RenderApi.h"    // for renderPage
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpHttpServer.h"
#include "mcp/McpProtocol.h"
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/PathText.h"
#include "model/Document.h"  // for Document
#include "model/XojPage.h"   // for XojPage

#include "EventCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::optional<size_t> pageFromUri(Control* ctrl, const std::string& uri, const std::string& suffix) {
    const std::string prefix = "xournal://page/";
    if (uri.rfind(prefix, 0) != 0 || uri.size() < prefix.size() + suffix.size() + 1) {
        return std::nullopt;
    }
    std::string rest = uri.substr(prefix.size());
    if (!suffix.empty()) {
        if (rest.size() <= suffix.size() || rest.compare(rest.size() - suffix.size(), suffix.size(), suffix) != 0) {
            return std::nullopt;
        }
        rest = rest.substr(0, rest.size() - suffix.size());
    }
    if (rest.empty() || rest.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }
    const size_t n = std::stoul(rest);
    if (n < 1 || n > ctrl->getDocument()->getPageCount()) {
        return std::nullopt;
    }
    return n - 1;
}

json text(const std::string& uri, const json& data) {
    return json::array({{{"uri", uri}, {"mimeType", "application/json"}, {"text", data.dump()}}});
}

}  // namespace

void registerResources(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    server.getRegistry().addResource({"xournal://document", "document",
                                      "The open document: path, pages, layers, current page (JSON)", "application/json",
                                      false, [ctrl](const std::string& uri) -> std::optional<json> {
                                          if (!ctrl->getWindow()) {
                                              return std::nullopt;
                                          }
                                          std::shared_lock lock(*ctrl->getDocument());
                                          Document* doc = ctrl->getDocument();
                                          json pages = json::array();
                                          for (size_t i = 0; i < doc->getPageCount(); i++) {
                                              pages.push_back(pageSummary(doc->getPage(i), i));
                                          }
                                          const auto path = doc->getFilepath();
                                          return text(uri, {{"path", path.empty() ? json(nullptr) : json(toUtf8(path))},
                                                            {"current_page", ctrl->getCurrentPageNo() + 1},
                                                            {"pages", pages}});
                                      }});
    server.getRegistry().addResource({"xournal://changes", "changes",
                                      "The latest document changes (user and agent), newest last (JSON)",
                                      "application/json", false, [srv](const std::string& uri) -> std::optional<json> {
                                          auto* hub = srv->getEvents();
                                          if (!hub) {
                                              return std::nullopt;
                                          }
                                          hub->flush();
                                          const uint64_t last = hub->lastSeq();
                                          json list = json::array();
                                          for (const auto& e: hub->since(last > 100 ? last - 100 : 0, 100)) {
                                              list.push_back(eventJson(e));
                                          }
                                          return text(uri, {{"cursor", last}, {"events", list}});
                                      }});
    server.getRegistry().addResource(
            {"xournal://page/{page}", "page", "Elements of a page (ids, types, bboxes, text) as JSON",
             "application/json", true, [ctrl](const std::string& uri) -> std::optional<json> {
                 auto page = pageFromUri(ctrl, uri, "");
                 if (!page) {
                     return std::nullopt;
                 }
                 std::shared_lock lock(*ctrl->getDocument());
                 json list = json::array();
                 for (const auto& loc:
                      api::elementsOnPage(ctrl->getDocument()->getPage(*page), *page, std::nullopt, std::nullopt)) {
                     list.push_back(elementToJson(loc, Detail::Bbox, 0));
                 }
                 return text(uri, {{"page", *page + 1}, {"elements", list}});
             }});
    server.getRegistry().addResource(
            {"xournal://page/{page}/image", "page image", "A PNG rendering of a page", "image/png", true,
             [ctrl](const std::string& uri) -> std::optional<json> {
                 auto page = pageFromUri(ctrl, uri, "/image");
                 if (!page) {
                     return std::nullopt;
                 }
                 api::RenderOptions o;
                 o.page = *page;
                 o.maxPixels = 1200;
                 const auto img = api::renderPage(ctrl->getDocument(), o);
                 return json::array({{{"uri", uri}, {"mimeType", "image/png"}, {"blob", base64Encode(img.png)}}});
             }});
}

void wireResourceNotifications(McpServer& server) {
    auto* hub = server.getEvents();
    if (!hub) {
        return;
    }
    McpServer* srv = &server;
    hub->setListener([srv](const api::DocEvent& e) {
        auto* http = srv->getHttpServer();
        if (!http) {
            return;
        }
        std::vector<std::string> uris = {"xournal://document", "xournal://changes"};
        if (e.type.rfind("element_", 0) == 0) {
            const std::string page = "xournal://page/" + std::to_string(e.page + 1);
            uris.push_back(page);
            uris.push_back(page + "/image");
        }
        for (const auto& uri: uris) {
            http->broadcast(rpc::notification("notifications/resources/updated", {{"uri", uri}}),
                            [&uri](const Session& s) { return s.subscriptions.count(uri) > 0; });
        }
    });
}

}  // namespace xoj::mcp::tools
