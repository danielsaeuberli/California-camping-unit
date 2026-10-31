"""Allowed keys of the hardware profile.

A dict value describes a nested mapping; "*" matches any key (named entries such
as ``outputs.roof_a``). ``LEAF`` accepts any scalar or list. Unknown keys are an error.
"""

from __future__ import annotations

from typing import Any

LEAF: Any = None

_OUTPUT = {"pin": LEAF, "channel": LEAF, "active": LEAF, "meaning": LEAF}
_INPUT = {
    "pin": LEAF,
    "channel": LEAF,
    "debounce_ms": LEAF,
    "mode": LEAF,
    "active": LEAF,
    "pulse_ms": LEAF,
    "frame_ms": LEAF,
    "log_only": LEAF,
    "window_ms": LEAF,
    "moving_min_edges": LEAF,
    "enabled": LEAF,
}
_ANALOG = {
    "pin": LEAF,
    "channel": LEAF,
    "divider": LEAF,
    "pullup_ohm": LEAF,
    "curve": LEAF,
    "measure_only": LEAF,
    "fallback_for": LEAF,
    "source": LEAF,
}
_CURVE = {"type": LEAF, "points": LEAF, "labels": LEAF, "ohm": LEAF, "hysteresis_pct": LEAF}

SCHEMA: dict[str, Any] = {
    "profile": LEAF,
    "schema_version": LEAF,
    "vehicle": {
        "vin_model_year": LEAF,
        "wiring_diagram": LEAF,
        "ignition_source": LEAF,
        "illumination_source": LEAF,
    },
    "buses": {
        "i2c": LEAF,
        "sm_h001": {"stack": LEAF, "ai_max_v": LEAF, "ai_input_ohm": LEAF},
        "sm_i009": {"stack": LEAF},
        "alive_gpio": LEAF,
        "wake_uart": {"device": LEAF, "baud": LEAF},
        "cerbo": {"host": LEAF, "port": LEAF, "keepalive_s": LEAF, "stale_s": LEAF, "portal_id": LEAF},
    },
    "outputs": {
        "roof_a": _OUTPUT,
        "roof_b": _OUTPUT,
        "fridge_req": _OUTPUT,
        "heater_splus": _OUTPUT,
        "heater_ladder": {
            "topology": LEAF,
            "base_ohm": LEAF,
            "gpio_active": LEAF,
            "segments": [{"channel": LEAF, "gpio": LEAF, "ohm": LEAF}],
            "switch_gap_ms": LEAF,
        },
    },
    "inputs": {"*": _INPUT},
    "analog": {"*": _ANALOG},
    "curves": {"open_ohm": LEAF, "short_ohm": LEAF, "*": _CURVE},
    "roof": {
        "truth_table": LEAF,
        "hold_timeout_ms": LEAF,
        "reverse_gap_ms": LEAF,
        "max_run_s": LEAF,
        "block_open_when_moving": LEAF,
        "require_ignition": LEAF,
    },
    "fridge": {
        "target_c": {"min": LEAF, "max": LEAF, "default": LEAF},
        "on_above_target_k": LEAF,
        "off_below_target_k": LEAF,
        "min_run_s": LEAF,
        "min_pause_s": LEAF,
        "fallback_duty": LEAF,
        "self_regulating_j1029": LEAF,
    },
    "heater": {
        "levels": LEAF,
        "level_to_c": LEAF,
        "start_timeout_s": LEAF,
        "afterrun_expected_s": LEAF,
    },
    "tanks": {"median_s": LEAF, "median_moving_s": LEAF, "stable_s": LEAF},
    "outside_temp_source": LEAF,
    "power": {
        "afterrun_to_standby_s": LEAF,
        "standby_to_shutdown_s": LEAF,
        "undervoltage_v": LEAF,
        "undervoltage_hold_s": LEAF,
        "heartbeat_s": LEAF,
    },
    "release": {"roof": LEAF, "fridge": LEAF, "heater": LEAF},
    "victron": {
        "control": LEAF,
        "modes": LEAF,
        "current_limit_a": {"min": LEAF, "max": LEAF},
        "confirm_s": LEAF,
        "min_interval_s": LEAF,
    },
    "ui": {"idle_return_min": LEAF, "language": LEAF, "theme": LEAF, "brightness_pct": LEAF, "backlight_control": LEAF},
    "measure": {"outputs_locked": LEAF, "di_resolution_ms": LEAF, "ai_rate_hz": LEAF},
}
