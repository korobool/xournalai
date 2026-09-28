#include "config-features.h"

#ifdef ENABLE_MCP

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "mcp/McpProtocol.h"
#include "mcp/Registry.h"
#include "mcp/Schema.h"

using namespace xoj::mcp;

namespace {

Registry makeRegistry() {
    Registry reg;
    ToolSpec echo;
    echo.name = "echo";
    echo.title = "Echo";
    echo.description = "Echoes its text argument";
    echo.inputSchema = schema::object({{"text", schema::string("Text to echo")}}, {"text"});
    echo.readOnly = true;
    echo.handler = [](const json& args) {
        if (!args.contains("text")) {
            throw ToolError("text is required");
        }
        return ToolResult::text(args["text"].get<std::string>());
    };
    reg.addTool(std::move(echo));

    ToolSpec boom;
    boom.name = "boom";
    boom.title = "Boom";
    boom.description = "Throws an unexpected exception";
    boom.inputSchema = schema::object({});
    boom.handler = [](const json&) -> ToolResult { throw std::runtime_error("kaputt"); };
    reg.addTool(std::move(boom));

    ToolSpec later;
    later.name = "later";
    later.title = "Later";
    later.description = "Replies asynchronously";
    later.inputSchema = schema::object({});
    later.tier = Tier::Draw;
    later.asyncHandler = [](const json&, Responder r) { r(ToolResult::structured({{"ok", true}})); };
    reg.addTool(std::move(later));

    PromptSpec p;
    p.name = "summarize";
    p.title = "Summarize";
    p.description = "Summarize a page";
    p.arguments = {{"page", "Page number", true}};
    p.render = [](const json& a) { return "Summarize page " + a["page"].get<std::string>(); };
    reg.addPrompt(std::move(p));

    ResourceSpec r;
    r.uri = "xournal://page/{page}";
    r.name = "page";
    r.description = "A page";
    r.mimeType = "application/json";
    r.isTemplate = true;
    r.read = [](const std::string& uri) -> std::optional<json> {
        if (uri.rfind("xournal://page/", 0) != 0) {
            return std::nullopt;
        }
        return std::optional<json>(std::in_place,
                                   json::array({{{"uri", uri}, {"mimeType", "application/json"}, {"text", "{}"}}}));
    };
    reg.addResource(std::move(r));
    return reg;
}

json request(int id, const std::string& method, json params = json::object()) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}};
}

struct Fixture {
    Registry reg = makeRegistry();
    McpProtocol proto{ServerInfo{"xournalai", "Xournal++", "9.9.9", "Be nice"}, reg};
    Session session;

    json call(const json& msg) {
        json out;
        bool replied = false;
        bool isRequest = proto.handle(msg, session, [&](json r) {
            out = std::move(r);
            replied = true;
        });
        EXPECT_EQ(isRequest, replied);
        return out;
    }
};

}  // namespace

TEST(McpProtocol, initializeNegotiatesVersion) {
    Fixture f;
    auto r = f.call(request(1, "initialize", {{"protocolVersion", "2025-03-26"}, {"clientInfo", {{"name", "t"}}}}));
    EXPECT_EQ(r["id"], 1);
    EXPECT_EQ(r["result"]["protocolVersion"], "2025-03-26");
    EXPECT_EQ(r["result"]["serverInfo"]["version"], "9.9.9");
    EXPECT_EQ(r["result"]["instructions"], "Be nice");
    EXPECT_TRUE(r["result"]["capabilities"].contains("tools"));

    auto r2 = f.call(request(2, "initialize", {{"protocolVersion", "1999-01-01"}}));
    EXPECT_EQ(r2["result"]["protocolVersion"], McpProtocol::LATEST_PROTOCOL_VERSION);
}

TEST(McpProtocol, notificationsGetNoReply) {
    Fixture f;
    json note = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
    bool replied = false;
    EXPECT_FALSE(f.proto.handle(note, f.session, [&](json) { replied = true; }));
    EXPECT_FALSE(replied);
    EXPECT_TRUE(f.session.initialized);
}

TEST(McpProtocol, toolsListHasSchemasAndAnnotations) {
    Fixture f;
    auto r = f.call(request(3, "tools/list"));
    ASSERT_EQ(r["result"]["tools"].size(), 3u);
    const auto& echo = r["result"]["tools"][0];
    EXPECT_EQ(echo["name"], "echo");
    EXPECT_EQ(echo["inputSchema"]["required"][0], "text");
    EXPECT_EQ(echo["annotations"]["readOnlyHint"], true);
}

TEST(McpProtocol, toolCallSuccessErrorAndBarrier) {
    Fixture f;
    auto ok = f.call(request(4, "tools/call", {{"name", "echo"}, {"arguments", {{"text", "hi"}}}}));
    EXPECT_EQ(ok["result"]["isError"], false);
    EXPECT_EQ(ok["result"]["content"][0]["text"], "hi");

    auto bad = f.call(request(5, "tools/call", {{"name", "echo"}, {"arguments", json::object()}}));
    EXPECT_EQ(bad["result"]["isError"], true);
    EXPECT_EQ(bad["result"]["content"][0]["text"], "text is required");

    auto boom = f.call(request(6, "tools/call", {{"name", "boom"}}));
    EXPECT_EQ(boom["result"]["isError"], true);
    EXPECT_NE(boom["result"]["content"][0]["text"].get<std::string>().find("kaputt"), std::string::npos);

    auto unknown = f.call(request(7, "tools/call", {{"name", "nope"}}));
    EXPECT_EQ(unknown["error"]["code"], rpc::INVALID_PARAMS);
}

TEST(McpProtocol, asyncToolAndStructuredContent) {
    Fixture f;
    auto r = f.call(request(8, "tools/call", {{"name", "later"}}));
    EXPECT_EQ(r["result"]["structuredContent"]["ok"], true);
}

TEST(McpProtocol, permissionCheckDeniesTier) {
    Fixture f;
    f.proto.setPermissionCheck([](const ToolSpec& t) -> std::optional<std::string> {
        if (t.tier == Tier::Draw) {
            return "draw tier not granted";
        }
        return std::nullopt;
    });
    auto r = f.call(request(9, "tools/call", {{"name", "later"}}));
    EXPECT_EQ(r["result"]["isError"], true);
    EXPECT_EQ(r["result"]["content"][0]["text"], "draw tier not granted");
}

TEST(McpProtocol, observerSeesCalls) {
    Fixture f;
    std::string seen;
    bool okSeen = true;
    f.proto.setCallObserver([&](const std::string& tool, bool ok, double) {
        seen = tool;
        okSeen = ok;
    });
    f.call(request(10, "tools/call", {{"name", "boom"}}));
    EXPECT_EQ(seen, "boom");
    EXPECT_FALSE(okSeen);
}

TEST(McpProtocol, promptsAndResources) {
    Fixture f;
    auto list = f.call(request(11, "prompts/list"));
    EXPECT_EQ(list["result"]["prompts"][0]["name"], "summarize");
    auto get = f.call(request(12, "prompts/get", {{"name", "summarize"}, {"arguments", {{"page", "3"}}}}));
    EXPECT_EQ(get["result"]["messages"][0]["content"]["text"], "Summarize page 3");
    auto missing = f.call(request(13, "prompts/get", {{"name", "summarize"}}));
    EXPECT_TRUE(missing.contains("error"));

    auto templates = f.call(request(14, "resources/templates/list"));
    EXPECT_EQ(templates["result"]["resourceTemplates"][0]["uriTemplate"], "xournal://page/{page}");
    auto read = f.call(request(15, "resources/read", {{"uri", "xournal://page/2"}}));
    EXPECT_EQ(read["result"]["contents"][0]["uri"], "xournal://page/2");
    auto notFound = f.call(request(16, "resources/read", {{"uri", "xournal://nothing"}}));
    EXPECT_EQ(notFound["error"]["code"], rpc::RESOURCE_NOT_FOUND);

    f.call(request(17, "resources/subscribe", {{"uri", "xournal://page/2"}}));
    EXPECT_EQ(f.session.subscriptions.count("xournal://page/2"), 1u);
}

TEST(McpProtocol, invalidMessages) {
    Fixture f;
    auto r = f.call(json::array());
    EXPECT_EQ(r["error"]["code"], rpc::INVALID_REQUEST);
    auto m = f.call(request(18, "does/not/exist"));
    EXPECT_EQ(m["error"]["code"], rpc::METHOD_NOT_FOUND);
    json clientResponse = {{"jsonrpc", "2.0"}, {"id", 5}, {"result", json::object()}};
    EXPECT_FALSE(f.proto.handle(clientResponse, f.session, [](json) {}));
}

TEST(McpSchema, lintAcceptsPortableSchemas) {
    auto s = schema::object({{"page", schema::integer("Page number (1-based)")},
                             {"points", schema::array("Points", schema::array("x,y pair", schema::number("coord")))},
                             {"mode", schema::enumeration("Mode", {"a", "b"})}},
                            {"page"});
    EXPECT_TRUE(schema::lint("page_render", s).empty());
}

TEST(McpSchema, lintRejectsNonPortableSchemas) {
    json s = {{"type", "object"},
              {"properties",
               {{"a", {{"type", "string"}}},
                {"b", {{"oneOf", json::array()}, {"description", "x"}}},
                {"c", {{"type", "array"}, {"description", "no items"}}}}},
              {"required", {"zzz"}},
              {"additionalProperties", false}};
    auto problems = schema::lint("Bad-Name", s);
    EXPECT_GE(problems.size(),
              5u);  // name, a needs description, b oneOf + type, c items, required, additionalProperties
    EXPECT_FALSE(schema::isValidToolName("Bad-Name"));
    EXPECT_TRUE(schema::isValidToolName("ui_menu_select"));
    EXPECT_FALSE(schema::isValidToolName("this_name_is_definitely_longer_than_forty_characters"));
}

TEST(McpRegistry, rejectsDuplicatesAndBadSchemas) {
    Registry reg = makeRegistry();
    ToolSpec dup;
    dup.name = "echo";
    dup.description = "dup";
    dup.inputSchema = schema::object({});
    dup.handler = [](const json&) { return ToolResult::text(""); };
    EXPECT_THROW(reg.addTool(dup), std::logic_error);
    dup.name = "bad";
    dup.inputSchema = {{"type", "object"}, {"properties", {{"x", {{"$ref", "#/y"}}}}}};
    EXPECT_THROW(reg.addTool(dup), std::logic_error);
}

#endif  // ENABLE_MCP
