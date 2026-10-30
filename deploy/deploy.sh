#!/usr/bin/env bash
# make deploy: copy a versioned release to /data/releases/<version>, switch the
# 'current' symlink and restart (SPEC 2.9). Rollback: point 'current' back.
set -euo pipefail
PI="${1:?usage: deploy.sh user@host version}"
VERSION="${2:?usage: deploy.sh user@host version}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="/data/releases/$VERSION"

rsync -a --delete \
  --exclude .git --exclude .venv --exclude var --exclude '__pycache__' \
  --exclude '.pytest_cache' --exclude '.mypy_cache' --exclude '.ruff_cache' \
  --exclude 'wake-mcu' --exclude 'ui/reference' \
  "$ROOT/" "$PI:$DEST/"

ssh "$PI" bash -s -- "$DEST" <<'REMOTE'
set -euo pipefail
DEST="$1"
cd "$DEST"
if [[ ! -d .venv ]]; then
  python3 -m venv .venv
  if [[ -f requirements.lock ]]; then
    .venv/bin/pip install --require-hashes -r requirements.lock
  else
    echo "WARNING: requirements.lock missing, installing unpinned" >&2
    .venv/bin/pip install .
  fi
  .venv/bin/pip install --no-deps -e .
fi
ln -sfn "$DEST" /data/releases/current
sudo systemctl restart camperd camper-ui
echo "running $(readlink /data/releases/current)"
REMOTE
