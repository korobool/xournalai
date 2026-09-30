"""E11: with share_recordings off, recordings are not announced."""

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"share_recordings": False}}


def test_recordings_stay_private_when_sharing_is_off(app):
    c = app.client()
    cursor = c.call("changes_get")["cursor"]
    assert not c.call("test_recording", file="/tmp/x.ogg", name="x.ogg", duration_ms=1000)["observed"]
    assert not [e for e in c.call("changes_get", since=cursor)["events"] if e["type"] == "audio_recorded"]
