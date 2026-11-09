"""E6: the companion folder is prepared for the serving session and keeps the user's own additions."""

import json


def test_companion_folder(app):
    c = app.client()
    d = app.home / "data" / "xournalai" / "companion"
    claude = (d / "CLAUDE.md").read_text()
    assert "xournalai's serving session" in claude and str(app.port) in claude and "`*w!`" in claude
    assert (d / "AGENTS.md").read_text() == claude
    mcp = json.loads((d / ".mcp.json").read_text())["mcpServers"]["xournalai"]
    assert mcp["args"] == ["--mcp-stdio", f"--mcp-port={app.port}"] and mcp["command"].endswith("xournalpp")
    for agent in ("canvas-quick", "canvas-artist"):
        text = (d / ".claude" / "agents" / f"{agent}.md").read_text()
        assert text.startswith(f"---\nname: {agent}\n") and "transaction_commit" in text
    assert "## You coordinate" in claude and "## Transactions" in claude
    # the skills the app ships, as the folder's own project skills (never in the user's ~/.claude: 2026-11-09)
    skill = (d / ".claude" / "skills" / "live-annotation" / "SKILL.md").read_text()
    assert skill.startswith("---\nname: live-annotation\n") and "live_notes(op=\"read\")" in skill
    assert "live-annotation" in claude
    assert (d / ".claude" / "skills" / "live-annotation" / "lib" / "inbox.py").exists()  # (its helpers too)
    assert "safe list" in claude and "never acted on" in claude  # (spoken requests: only the skill's safe list)
    local = json.loads((d / ".claude" / "settings.local.json").read_text())
    assert local["enableAllProjectMcpServers"] is True and "xournalai" in local["enabledMcpjsonServers"]
    # the user's own notes below the managed block survive a refresh (the server re-provisions on restart)
    (d / "CLAUDE.md").write_text(claude + "\n# Mine\nkeep this line\n")
    (d / ".mcp.json").write_text(json.dumps({"mcpServers": {"other": {"command": "x"}, "xournalai": {"command": "old"}}}))
    app.user_key("alt+a")  # AI Agent menu → settings → save restarts the server, which refreshes the folder
    import time
    time.sleep(0.4)
    app.user_key("s")
    time.sleep(0.8)
    app.user_key("alt+s", window="AI Agent Settings")
    time.sleep(1.0)
    refreshed = (d / "CLAUDE.md").read_text()
    assert "keep this line" in refreshed and refreshed.count("xournalai's serving session") == 1
    servers = json.loads((d / ".mcp.json").read_text())["mcpServers"]
    assert "other" in servers and servers["xournalai"]["command"].endswith("xournalpp")


def test_a_test_app_never_starts_a_real_agent(app):
    """2026-11-09: a real Claude Code started with a test app's XDG_DATA_HOME re-pointed the owner's ~/.local/bin/claude
    into the test folder. The harness puts stubs first on the app's PATH."""
    import subprocess
    out = subprocess.run(["sh", "-c", "command -v claude"], env=app.app_env() if hasattr(app, "app_env") else app.env,
                         capture_output=True, text=True).stdout.strip()
    assert out.startswith(str(app.home)), out
