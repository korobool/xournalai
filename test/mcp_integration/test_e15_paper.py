"""E15: the pale 2 mm grid is the paper of new pages, the user's and the agents'. On 2026-10-21 an agent's page_manage
insert with background "graph" gave a page the coarse 5 mm grid, and no tool could set the pale one: the agent patched
the saved file."""

import xoai

PALE = "r1=5.669,lw=0.2,f1=0xf3f3f3,af1=0x383838"
FORMER = "r1=7.087,lw=0.35,f1=0xe0e0e0,af1=0x383838"  # (the default until 1.15.2, "Fine graph (2.5 mm, light)")


def paper(c, page):
    return c.call("doc_info")["pages"][page - 1]["background"]


def test_1_new_documents_get_the_pale_grid(app):
    bg = paper(app.client(), 1)
    assert bg["type"] == "graph" and bg["config"] == PALE, bg


def test_2_it_is_in_the_paper_list(app):
    paths = [e["path"] for e in app.client().call("ui_menu_tree")["entries"]]
    assert any(p.endswith("/Pale graph (2 mm)") for p in paths), [p for p in paths if "raph" in p]
    assert any(p.endswith("/Fine graph (2.5 mm, light)") for p in paths)  # (the former default stays in the list)


def test_3_an_agent_s_graph_page_is_the_pale_grid(app):
    c = app.client()
    c.call("page_manage", op="background", page=1, background="plain")
    r = c.call("page_manage", op="insert", page=1, background="graph")
    assert r["background"]["type"] == "graph" and r["background"]["config"] == PALE, r["background"]
    r = c.call("page_manage", op="insert", page=2)  # (no paper given: like the page before it)
    assert r["background"]["config"] == PALE, r["background"]
    r = c.call("page_manage", op="background", page=1, background="graph")
    assert r["background"]["config"] == PALE, r["background"]
    assert "config" not in c.call("page_manage", op="background", page=1, background="dotted")["background"]


def test_4_an_agent_can_set_any_grid(app):
    c = app.client()
    r = c.call("page_manage", op="background", page=1, background="graph", background_config="r1=14.17,lw=0.5")
    assert r["background"]["config"] == "r1=14.17,lw=0.5", r["background"]
    r = c.call("page_manage", op="insert", page=1, background="graph", background_config="r1=8.5")
    assert r["page"] == 2 and r["background"]["config"] == "r1=8.5", r
    r = c.call("page_manage", op="background", page=1, background="graph", background_config="")
    assert "config" not in r["background"], r["background"]  # (the classic 5 mm Graph)
    for bad in ["r1=0", "r1=0.01", "r1=abc", "lw=500"]:
        assert "background_config" in c.call_error("page_manage", op="background", page=1, background="graph",
                                                   background_config=bad), bad
    assert "needs 'background'" in c.call_error("page_manage", op="background", page=1, background_config="r1=9")
    assert paper(c, 1).get("config") is None  # (nothing was changed by the refused calls)


def test_5_a_template_on_the_former_default_moves_to_it(app):
    """The owner's settings.xml still had the 2.5 mm fine graph (the default until 1.15.2) as the paper of new
    documents; another choice of theirs is kept."""
    for config, expected in [(FORMER, PALE), ("r1=10,lw=1", "r1=10,lw=1")]:
        other = xoai.App()
        other.env  # (creates its XDG directories)
        settings = other.home / "config" / "xournalpp" / "settings.xml"
        settings.parent.mkdir(parents=True, exist_ok=True)
        template = ("xoj/template&#10;copyLastPageSize=false&#10;copyLastPageSettings=true&#10;"
                    "size=595.275591x841.889764&#10;backgroundType=graph&#10;"
                    f"backgroundTypeConfig={config}&#10;backgroundColor=#ffffff&#10;")
        settings.write_text(f'<?xml version="1.0"?>\n<settings>\n  <property name="pageTemplate" '
                            f'value="{template}"/>\n</settings>\n')
        with other:
            c = other.client()
            assert paper(c, 1)["config"] == expected, (config, paper(c, 1))
            assert c.call("page_manage", op="insert", page=1, background="graph")["background"]["config"] == expected
