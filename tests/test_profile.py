import pytest
import yaml

from camperd.profile import UNCALIBRATED, Profile, ProfileError, is_cal, load_profile
from tests.conftest import PROFILE


def test_real_profile_loads(profile):
    assert profile.get("profile") == "t6-california-a8l-2018"
    assert profile.get("roof.truth_table") is UNCALIBRATED
    assert not is_cal(profile.get("analog.fridge_temp.pullup_ohm"))
    assert profile.channel_index("inputs.ign_15a", "DI") == 10
    assert profile.channel_index("outputs.heater_splus", "K") == 4


def test_unknown_key_rejected():
    data = yaml.safe_load(PROFILE.read_text())
    data["roof"]["max_run_sec"] = 60
    with pytest.raises(ProfileError, match="roof.max_run_sec"):
        Profile(data)


def test_unknown_top_level_key_rejected():
    data = yaml.safe_load(PROFILE.read_text())
    data["roooof"] = 1
    with pytest.raises(ProfileError):
        Profile(data)


def test_overlay_cannot_add_unknown_keys(tmp_path):
    bad = tmp_path / "o.yaml"
    bad.write_text("schema_version: 1\nroof: {speed: 3}\n")
    with pytest.raises(ProfileError):
        load_profile(PROFILE, overlay=bad)


def test_sim_overlay_does_not_touch_template(sim_profile, profile):
    assert sim_profile.get("roof.truth_table")["10"] == "open"
    assert profile.get("roof.truth_table") is UNCALIBRATED


def test_template_has_no_invented_values():
    text = PROFILE.read_text()
    for key in ("truth_table", "level_to_c", "self_regulating_j1029"):
        line = next(ln for ln in text.splitlines() if ln.strip().startswith(key + ":"))
        assert "TODO_MEASURE" in line
