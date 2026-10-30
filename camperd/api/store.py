"""Persistent UI settings and the event log on the data partition."""

from __future__ import annotations

import contextlib
import json
import time
from pathlib import Path
from typing import Any

from camperd.profile import Profile

THEMES = ("auto", "light", "dark")
IDLE_CHOICES = (1, 2, 5)
EVENTS_MAX_BYTES = 10 * 1024 * 1024


class Settings:
    def __init__(self, path: Path, profile: Profile) -> None:
        self.path = path
        self.values: dict[str, Any] = {
            "brightness": int(profile.get("ui.brightness_pct", 80)),
            "theme": str(profile.get("ui.theme", "auto")),
            "idle_min": int(profile.get("ui.idle_return_min", 2)),
        }
        self.backlight_control = profile.get("ui.backlight_control", "none")
        with contextlib.suppress(OSError, ValueError):
            self.values.update(self._validate(json.loads(path.read_text())))

    @staticmethod
    def _validate(values: dict[str, Any]) -> dict[str, Any]:
        out: dict[str, Any] = {}
        if "brightness" in values:
            b = int(values["brightness"])
            if not 10 <= b <= 100:
                raise ValueError("brightness out of range")
            out["brightness"] = b
        if "theme" in values:
            if values["theme"] not in THEMES:
                raise ValueError("bad theme")
            out["theme"] = values["theme"]
        if "idle_min" in values:
            if int(values["idle_min"]) not in IDLE_CHOICES:
                raise ValueError("bad idle_min")
            out["idle_min"] = int(values["idle_min"])
        unknown = set(values) - {"brightness", "theme", "idle_min"}
        if unknown:
            raise ValueError(f"unknown settings: {sorted(unknown)}")
        return out

    def update(self, values: dict[str, Any]) -> None:
        self.values.update(self._validate(values))
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self.values))
        tmp.replace(self.path)
        # OPEN: O10 - brightness is stored but has no effect until the backlight path is known.

    def public(self) -> dict[str, Any]:
        return {**self.values, "backlight_control": self.backlight_control}


class EventLog:
    """JSON lines, rotated to ``events.jsonl.1`` at 10 MB."""

    def __init__(self, path: Path) -> None:
        self.path = path
        path.parent.mkdir(parents=True, exist_ok=True)

    def write(self, kind: str, **data: Any) -> None:
        line = json.dumps({"ts": round(time.time(), 3), "kind": kind, **data}, ensure_ascii=False)
        try:
            if self.path.exists() and self.path.stat().st_size > EVENTS_MAX_BYTES:
                self.path.replace(self.path.with_suffix(".jsonl.1"))
            with self.path.open("a", encoding="utf-8") as fh:
                fh.write(line + "\n")
        except OSError:
            pass  # logging must never stop the control loop
