"""Load and validate the YAML hardware profile.

``TODO_MEASURE`` and ``TODO_CHECK`` become :data:`UNCALIBRATED`. Code must treat such
values as missing ("nicht kalibriert"), never substitute an estimate.
"""

from __future__ import annotations

import copy
from pathlib import Path
from typing import Any

import yaml

from camperd.profile.schema import SCHEMA

PLACEHOLDERS = ("TODO_MEASURE", "TODO_CHECK")


class ProfileError(ValueError):
    pass


class _Uncalibrated:
    _instance: _Uncalibrated | None = None

    def __new__(cls) -> _Uncalibrated:
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __repr__(self) -> str:
        return "UNCALIBRATED"

    def __bool__(self) -> bool:
        return False


UNCALIBRATED: Any = _Uncalibrated()


def is_cal(value: Any) -> bool:
    return value is not UNCALIBRATED and value is not None


def _replace_placeholders(node: Any) -> Any:
    if isinstance(node, dict):
        return {k: _replace_placeholders(v) for k, v in node.items()}
    if isinstance(node, list):
        return [_replace_placeholders(v) for v in node]
    if isinstance(node, str) and node in PLACEHOLDERS:
        return UNCALIBRATED
    return node


def _validate(node: Any, schema: Any, path: str) -> None:
    if schema is None:
        return
    if isinstance(schema, list):
        if not isinstance(node, list):
            raise ProfileError(f"{path}: expected a list")
        for i, item in enumerate(node):
            _validate(item, schema[0], f"{path}[{i}]")
        return
    if not isinstance(node, dict):
        raise ProfileError(f"{path}: expected a mapping")
    for key, value in node.items():
        if key in schema:
            _validate(value, schema[key], f"{path}.{key}" if path else str(key))
        elif "*" in schema:
            _validate(value, schema["*"], f"{path}.{key}")
        else:
            raise ProfileError(f"unknown profile key: {path}.{key}" if path else f"unknown profile key: {key}")


def _merge(base: dict[str, Any], overlay: dict[str, Any]) -> dict[str, Any]:
    out = copy.deepcopy(base)
    for key, value in overlay.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = _merge(out[key], value)
        else:
            out[key] = copy.deepcopy(value)
    return out


class Profile:
    """Validated profile with dotted-path access."""

    def __init__(self, data: dict[str, Any], source: str = "<memory>") -> None:
        _validate(data, SCHEMA, "")
        if data.get("schema_version") != 1:
            raise ProfileError(f"unsupported schema_version: {data.get('schema_version')!r}")
        self.raw = _replace_placeholders(data)
        self.source = source

    def get(self, dotted: str, default: Any = UNCALIBRATED) -> Any:
        node: Any = self.raw
        for part in dotted.split("."):
            if not isinstance(node, dict) or part not in node:
                return default
            node = node[part]
        return node

    def require(self, dotted: str) -> Any:
        value = self.get(dotted, default=_MISSING)
        if value is _MISSING:
            raise ProfileError(f"missing profile key: {dotted}")
        return value

    def channel_index(self, dotted: str, prefix: str) -> int:
        """``inputs.ign_15a`` with prefix ``DI`` -> 10 (1-based channel number)."""
        ch = self.require(f"{dotted}.channel")
        if not isinstance(ch, str) or not ch.startswith(prefix):
            raise ProfileError(f"{dotted}.channel: expected {prefix}<n>, got {ch!r}")
        return int(ch[len(prefix):])


_MISSING = object()


def load_profile(path: str | Path, overlay: str | Path | None = None) -> Profile:
    data = _read(path)
    source = str(path)
    if overlay is not None:
        extra = _read(overlay)
        _validate(extra, SCHEMA, "")
        data = _merge(data, extra)
        source += f" + {overlay}"
    return Profile(data, source)


def _read(path: str | Path) -> dict[str, Any]:
    with open(path, encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    if not isinstance(data, dict):
        raise ProfileError(f"{path}: profile must be a mapping")
    return data
