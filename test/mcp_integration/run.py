#!/usr/bin/env python3
"""Runs the xournalai MCP integration scenarios against the built application.

    python3 test/mcp_integration/run.py            # all test_*.py files
    python3 test/mcp_integration/run.py e0 -k doc  # files matching "e0", tests whose name contains "doc"

Each scenario is a function `test_*(app)` that receives a running `xoai.App` (a fresh application per test file).
Output is also written to build/integration.log (shown on the progress board).
"""

import argparse
import importlib.util
import pathlib
import sys
import time
import traceback

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import xoai  # noqa: E402

LOG = xoai.ROOT / "build" / "integration.log"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*", help="substrings of scenario file names")
    ap.add_argument("-k", default="", help="substring of test names")
    opts = ap.parse_args()

    files = sorted(HERE.glob("test_*.py"))
    if opts.files:
        files = [f for f in files if any(s in f.name for s in opts.files)]
    LOG.parent.mkdir(exist_ok=True)
    log = LOG.open("w")

    def out(line=""):
        print(line, flush=True)
        log.write(line + "\n")
        log.flush()

    passed = failed = 0
    for f in files:
        spec = importlib.util.spec_from_file_location(f.stem, f)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        tests = [(n, fn) for n, fn in vars(mod).items() if n.startswith("test_") and callable(fn) and opts.k in n]
        if not tests:
            continue
        out(f"== {f.name}")
        args = getattr(mod, "APP_ARGS", [])
        with xoai.App(*args, permissions=getattr(mod, "APP_PERMISSIONS", None),
                      config=getattr(mod, "APP_CONFIG", None), env=getattr(mod, "APP_ENV", None)) as app:
            for name, fn in tests:
                t = time.time()
                try:
                    fn(app)
                    passed += 1
                    out(f"  PASS {name} ({time.time() - t:.1f}s)")
                except Exception:
                    failed += 1
                    out(f"  FAIL {name}")
                    out("    " + traceback.format_exc().replace("\n", "\n    "))
                    noise = ("ALSA lib", "No device found", "Probably this is the reason", "xopp-WARNING")
                    tail = [l for l in app.read_log().splitlines() if l.strip() and not l.startswith(noise)]
                    out("    app log tail:\n      " + "\n      ".join(tail[-15:]))
    out(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
