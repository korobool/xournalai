"""E5: a second xournalai window started while the first holds the port takes it over when the first is closed."""

import time

import xoai


def test_second_window_takes_over_the_port(app):
    second = xoai.App()
    second.port = app.port  # same port, like two windows started from the launcher
    second.start()  # "comes up" at once: the first window answers on the port
    try:
        first = app.client()
        assert first.call("app_status")["app"] == "xournalai"
        time.sleep(1)
        assert "retrying" in second.read_log()  # the second one could not listen and keeps trying
        app.stop()  # the user closes the first window
        second.token = __import__("json").loads(second.config_file.read_text())["token"]
        for _ in range(40):
            try:
                c = second.client()
                break
            except Exception:
                time.sleep(0.25)
        else:
            raise AssertionError("the second window never took over the port:\n" + second.read_log()[-1500:])
        assert c.call("app_status")["app"] == "xournalai"
        assert "listening on" in second.read_log()
    finally:
        second.stop()
