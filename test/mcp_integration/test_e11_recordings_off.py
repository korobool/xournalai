"""E11: with share_recordings off, recordings are not announced. (Since 1.16.0 the app always hears that a recording
ended, to end a live-notes session with it; it tells nobody.)"""

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"share_recordings": False}}


def test_recordings_stay_private_when_sharing_is_off(app):
    c = app.client()
    cursor = c.call("changes_get")["cursor"]
    c.call("test_recording", file="/tmp/x.ogg", name="x.ogg", duration_ms=1000)
    assert not [e for e in c.call("changes_get", since=cursor)["events"] if e["type"] == "audio_recorded"]
    st = c.call("app_status")["recordings"]
    assert st["pending"] == 0 and not st["chooser"], st  # (no chooser either: nothing goes anywhere)
