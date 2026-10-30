PY ?= .venv/bin/python
PI ?= camper@192.168.77.2
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

.PHONY: venv dev dev-uncal test lint deploy clean

venv:
	python3 -m venv .venv
	.venv/bin/pip install -r requirements-dev.txt

dev:  ## camperd with the sim HAL, UI on http://localhost:8080 (simulator controls on /sim)
	$(PY) -m camperd --hal sim --no-watchdog

dev-uncal:  ## simulator without the invented sim calibration
	$(PY) -m camperd --hal sim --no-watchdog --sim-uncalibrated

test:
	$(PY) -m pytest -q

lint:
	$(PY) -m ruff check camperd tests tools
	$(PY) -m mypy

deploy:  ## versioned release on /data/releases, symlink switch, restart (SPEC 2.9)
	deploy/deploy.sh $(PI) $(VERSION)

clean:
	rm -rf var .pytest_cache .mypy_cache .ruff_cache
