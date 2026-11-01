# Camping Unit (SPEC 11): host tests and simulator, device build via ESP-IDF.
BUILD ?= build/host
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: test sim sim-shot build flash fonts clean

$(BUILD)/build.ninja:
	cmake -S host -B $(BUILD) -G Ninja -DCMAKE_BUILD_TYPE=Debug

test: $(BUILD)/build.ninja
	cmake --build $(BUILD) -j $(JOBS)
	ctest --test-dir $(BUILD) --output-on-failure

sim: $(BUILD)/build.ninja
	cmake --build $(BUILD) -j $(JOBS) --target camping_sim
	$(BUILD)/camping_sim

# Headless screenshots for review (SDL dummy driver).
sim-shot: $(BUILD)/build.ninja
	cmake --build $(BUILD) -j $(JOBS) --target camping_sim
	SDL_VIDEODRIVER=dummy $(BUILD)/camping_sim --shot build/sim-overview.bmp --page overview
	SDL_VIDEODRIVER=dummy $(BUILD)/camping_sim --shot build/sim-roof.bmp --page roof

build:
	cd firmware && idf.py build

flash:
	cd firmware && idf.py flash monitor

fonts:
	tools/gen_fonts.sh

clean:
	rm -rf build
