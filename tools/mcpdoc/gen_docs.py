#!/usr/bin/env python3
"""Generates docs/mcp/TOOLS.md and docs/mcp/COVERAGE.md from a running xournalai (the installed build, headless).

    python3 tools/mcpdoc/gen_docs.py

Everything in TOOLS.md comes from the server itself (tools/list, prompts/list, resources), so it cannot drift from
the code. COVERAGE.md combines the live action and menu lists with the hand-kept maps below.
"""

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "test" / "mcp_integration"))
import xoai  # noqa: E402

DOCS = ROOT / "docs" / "mcp"
TIER_ORDER = ["read", "draw", "ui", "files", "destructive"]
TIER_TEXT = {
    "read": "inspect and render the document",
    "draw": "create and edit content",
    "ui": "menus, tools, dialogs, view",
    "files": "open, save, import, export",
    "destructive": "discard unsaved work, overwrite files, quit (off by default)",
}

# Lua plugin API (plugins/luapi_application.def.lua) -> the MCP way to do the same
LUA = {
    "glib": "— (Lua runtime helper)",
    "glib_rename": "— (Lua runtime helper)",
    "saveAs": "`file_save_as`",
    "fileDialogSave": "`ui_file_chooser` (agents pass paths directly)",
    "getFilePath": "`file_info`, `app_status`",
    "fileDialogOpen": "`ui_file_chooser` (agents pass paths directly)",
    "msgbox": "`show_message` (non-modal callout)",
    "openDialog": "`show_message`; dialogs of the app via `ui_*`",
    "registerUi": "— (plugins add menu entries; agents call tools instead)",
    "changeActionState": "`action_run(state=...)`",
    "getActionState": "`actions_list`",
    "activateAction": "`action_run`",
    "uiAction": "`action_run`",
    "sidebarAction": "`action_run`, `ui_interact`",
    "getSidebarPageNo": "`ui_inspect`",
    "setSidebarPageNo": "`ui_interact`",
    "layerAction": "`layer_manage`, `action_run`",
    "showFloatingToolbox": "`action_run`",
    "addSplines": "`create_from_svg` (SVG path data with cubic Béziers), `create_shapes`",
    "addStrokes": "`create_strokes`, `pen_draw`",
    "addTexts": "`create_text`",
    "getTexts": "`page_elements(types=[text])`",
    "addLinks": "`create_link`",
    "getLinks": "`page_elements`",
    "getStrokes": "`page_elements(detail=full)`",
    "refreshPage": "— (automatic)",
    "changeCurrentPageBackground": "`page_manage(op=background)`",
    "getColorPalette": "`tool_set`, `guide(drawing)`",
    "changeToolColor": "`tool_set`",
    "changeBackgroundPdfPageNr": "`page_manage(op=background)`",
    "getToolInfo": "`app_status`, `view` (user_tool)",
    "getDocumentStructure": "`doc_info`",
    "scrollToPage": "`view`, `page_manage(op=goto)`",
    "scrollToPos": "`view`",
    "getScrollPos": "`view`",
    "getPageLabel": "`doc_info`",
    "setCurrentPage": "`page_manage(op=goto)`",
    "setPageSize": "`page_manage(op=size)`",
    "setCurrentLayer": "`layer_manage`",
    "setLayerVisibility": "`layer_manage`",
    "setCurrentLayerName": "`layer_manage`",
    "setBackgroundName": "`page_manage(op=background)`",
    "getDisplayDpi": "`view`",
    "getZoom": "`view`",
    "setZoom": "`view`",
    "export": "`export`",
    "openFile": "`file_open`",
    "addImages": "`import`",
    "getImages": "`page_elements(types=[image])`, `page_render`",
    "getFolder": "— (agents pass paths directly)",
    "clearSelection": "`elements_select(clear)`",
    "addToSelection": "`elements_select`",
    "registerPlaceholder": "— (toolbar placeholders are for plugins)",
    "setPlaceholderValue": "— (toolbar placeholders are for plugins)",
    "getFonts": "`create_text` (font by name)",
    "getFont": "`app_status`",
    "setFont": "`create_text(font=...)`, `tool_set`",
}

# Dialogs: how to open them and whether an integration test operates them
DIALOGS = [
    ("Open / Save As / Export file choosers", "File menu, `ui_menu_select`", "`ui_file_chooser`", "test_e3_dialogs"),
    ("Unsaved changes (Save / Discard / Cancel)", "opening or creating a file", "`ui_interact`; Discard needs "
     "`destructive`", "test_e3_dialogs, test_e3_safety"),
    ("Go to page", "Navigation/Goto Page", "`ui_inspect`, `ui_interact`", "test_e3_dialogs, test_e3_menus"),
    ("Export (PDF/PNG/SVG settings)", "File/Export as…", "`ui_*`; or the `export` tool directly", "—"),
    ("Page format / background color / templates", "Journal menu", "`ui_*`; or `page_manage` directly", "—"),
    ("Rename layer", "layer menu", "`ui_*`; or `layer_manage(op=rename)`", "—"),
    ("LaTeX editor", "Tools/LaTeX", "`ui_*`; or `create_latex` directly", "—"),
    ("Link", "Tools/Link", "`ui_*`; or `create_link` directly", "—"),
    ("Preferences", "Edit/Preferences", "`ui_inspect`, `ui_interact`", "—"),
    ("Plugin manager, toolbar customization, about", "menus", "`ui_inspect`, `ui_interact`", "—"),
    ("AI Agent Settings", "AI Agent/Settings…", "user only: agents are refused", "test_e4_ui"),
]


def schema_type(p):
    t = p.get("type", "")
    if t == "array":
        return f"array of {schema_type(p.get('items', {}))}"
    if "enum" in p:
        return " \\| ".join(f"`{v}`" for v in p["enum"])
    return t


def tools_md(tools, prompts, resources, templates, version):
    by_tier = {}
    for t in tools.values():
        by_tier.setdefault(t.get("_meta", {}).get("xournalai/tier", "?"), []).append(t)
    out = [
        "# xournalai MCP tool reference",
        "",
        f"Generated by `tools/mcpdoc/gen_docs.py` from xournalai {version}: {len(tools)} tools, "
        f"{len(prompts)} prompts, {len(resources) + len(templates)} resources. Do not edit by hand.",
        "",
        "Coordinates are page points (1/72 inch), origin top-left of each page; pages and layers are numbered "
        "from 1. Every tool belongs to a permission tier the user grants in the AI Agent settings.",
        "",
        "| Tier | Allows | Tools |",
        "|---|---|---|",
    ]
    for tier in TIER_ORDER:
        names = sorted(t["name"] for t in by_tier.get(tier, []))
        out.append(f"| `{tier}` | {TIER_TEXT[tier]} | {', '.join(f'[`{n}`](#{n})' for n in names) or '—'} |")
    for tier in TIER_ORDER:
        if not by_tier.get(tier):
            continue
        out += ["", f"## Tier `{tier}`"]
        for t in sorted(by_tier[tier], key=lambda x: x["name"]):
            a = t.get("annotations", {})
            flags = [f for f, k in (("read-only", "readOnlyHint"), ("idempotent", "idempotentHint"),
                                    ("destructive", "destructiveHint")) if a.get(k)]
            out += ["", f"### {t['name']}", "", f"**{t.get('title', '')}**" + (f" · {', '.join(flags)}" if flags
                                                                                  else ""), "", t["description"]]
            props = t["inputSchema"].get("properties", {})
            required = set(t["inputSchema"].get("required", []))
            if props:
                out += ["", "| Argument | Type | Required | Default | Description |", "|---|---|---|---|---|"]
                for name, p in props.items():
                    default = json.dumps(p["default"]) if "default" in p else ""
                    desc = p.get("description", "").replace("|", "\\|").replace("\n", " ")
                    out.append(f"| `{name}` | {schema_type(p)} | {'yes' if name in required else ''} | "
                               f"{default and '`' + default + '`'} | {desc} |")
    out += ["", "## Prompts", "", "| Prompt | Description | Arguments |", "|---|---|---|"]
    for p in prompts:
        args = ", ".join(f"`{a['name']}`" for a in p.get("arguments", []))
        out.append(f"| `{p['name']}` | {p.get('description', '')} | {args} |")
    out += ["", "## Resources", "", "| URI | Type | Description |", "|---|---|---|"]
    for r in resources:
        out.append(f"| `{r['uri']}` | {r.get('mimeType', '')} | {r.get('description', '')} |")
    for r in templates:
        out.append(f"| `{r['uriTemplate']}` | {r.get('mimeType', '')} | {r.get('description', '')} |")
    out += ["", "Resources support `resources/subscribe`; updates arrive as `notifications/resources/updated` "
            "on the SSE stream (and through `xournalpp --mcp-stdio`)."]
    return "\n".join(out) + "\n"


def coverage_md(actions, menu, version):
    user_only = {"win.mcp-paused", "win.mcp-settings"}
    out = [
        "# xournalai coverage",
        "",
        f"Generated by `tools/mcpdoc/gen_docs.py` from xournalai {version}. What of the application an agent can "
        "reach, and how.",
        "",
        "## Actions",
        "",
        f"All {len(actions)} application actions (the ones behind menus, toolbars and shortcuts) run through "
        "`action_run`; `actions_list` shows their state. Quitting needs the `destructive` tier; pausing the "
        "agent and its settings are for the user only.",
        "",
        "| Action | Menu | Agent access |",
        "|---|---|---|",
    ]
    for a in sorted(actions, key=lambda x: x["name"]):
        access = "user only" if a["name"] in user_only else (
            "`action_run` + `destructive`" if a["name"] == "app.quit" else "`action_run`")
        out.append(f"| `{a['name']}` | {a.get('menu', '') or '—'} | {access} |")
    entries = [e for e in menu if not e.get("submenu")]
    with_action = [e for e in entries if e.get("action")]
    out += [
        "",
        "## Main menu",
        "",
        f"{len(entries)} menu entries, {len(with_action)} with an action; every entry can be chosen by path with "
        "`ui_menu_select` (visibly, level by level) and is listed by `ui_menu_tree` (with shortcut, enabled and "
        "checked state). Entries without an action are filled at run time (recent files, toolbars, plugins) and "
        "are operated with `ui_inspect` / `ui_interact`.",
        "",
        "## Dialogs",
        "",
        "Any open window is listed by `ui_windows`, its widgets by `ui_inspect` (stable ids, labels, values), "
        "operated with `ui_interact` / `ui_keys`, seen with `ui_screenshot`.",
        "",
        "| Dialog | Opened from | Operated with | Tested in |",
        "|---|---|---|---|",
    ]
    out += [f"| {d} | {o} | {w} | {t} |" for d, o, w, t in DIALOGS]
    out += ["", "## Lua plugin API", "", "Every `app.*` function of the Lua plugin API and its MCP counterpart.", "",
            "| Lua | MCP |", "|---|---|"]
    lua_names = []
    for line in (ROOT / "plugins" / "luapi_application.def.lua").read_text().splitlines():
        if line.startswith("function app."):
            lua_names.append(line[len("function app."):].split("(")[0])
    missing = [n for n in lua_names if n not in LUA]
    for n in lua_names:
        out.append(f"| `app.{n}` | {LUA.get(n, '**not mapped**')} |")
    if missing:
        print("warning: Lua functions without a mapping:", ", ".join(missing), file=sys.stderr)
    return "\n".join(out) + "\n"


def main():
    with xoai.App() as app:
        c = app.client()
        tools = c.tools()
        prompts = c.request("prompts/list")["prompts"]
        resources = c.request("resources/list")["resources"]
        templates = c.request("resources/templates/list")["resourceTemplates"]
        version = c.call("app_status")["version"]
        actions = c.call("actions_list")["actions"]
        menu = c.call("ui_menu_tree")["entries"]
    import re
    names = set(tools) | set(TIER_ORDER)
    mentioned = set()
    for text in list(LUA.values()) + [" ".join(d) for d in DIALOGS]:
        mentioned |= {m.split("(")[0] for m in re.findall(r"`([a-z_]+)(?:\(|`)", text)}
    unknown = sorted(m for m in mentioned if m not in names)
    if unknown:
        sys.exit("unknown tools referenced in the coverage maps: " + ", ".join(unknown))
    (DOCS / "TOOLS.md").write_text(tools_md(tools, prompts, resources, templates, version))
    (DOCS / "COVERAGE.md").write_text(coverage_md(actions, menu, version))
    print(f"wrote TOOLS.md ({len(tools)} tools) and COVERAGE.md ({len(actions)} actions)")


if __name__ == "__main__":
    main()
