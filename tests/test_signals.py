import math

from camperd.signals import (
    Debouncer,
    EdgeCounter,
    MedianFilter,
    PulseDecoder,
    Quality,
    SteinhartHart,
    StepCurve,
    classify_resistance,
    resistance_to_voltage,
    sensor_resistance,
)


def test_debouncer():
    d = Debouncer(0.05)
    assert d.update(False, 0.0) is False
    assert d.update(True, 0.01) is False
    assert d.update(True, 0.05) is False
    assert d.update(True, 0.07) is True
    assert d.update(False, 0.08) is True  # glitch
    assert d.update(True, 0.09) is True


def test_edge_counter_window():
    e = EdgeCounter(1.0)
    n = 0
    for i in range(10):
        n = e.update(i % 2 == 0, i * 0.1)
    assert n == 9
    assert e.update(False, 2.5) <= 1


def test_median():
    m = MedianFilter(5)
    for t, v in enumerate([1, 1, 100, 1, 1]):
        out = m.update(v, t)
    assert out == 1


def test_resistance_roundtrip():
    v = resistance_to_voltage(2200, 5.1, 10000)
    assert math.isclose(sensor_resistance(v, 5.1, 10000), 2200, rel_tol=1e-9)
    assert sensor_resistance(5.1, 5.1, 1000) == math.inf
    assert classify_resistance(60000, 50000, 5) is Quality.OPEN
    assert classify_resistance(2, 50000, 5) is Quality.SHORT


def test_steinhart_hart_hits_calibration_points():
    pts = [[32650, 0], [10000, 25], [3602, 50]]
    sh = SteinhartHart(pts)
    for r, t in pts:
        assert abs(sh.temp_c(r) - t) < 1e-6
    assert abs(sh.temp_c(sh.ohm(4.8)) - 4.8) < 1e-3


def test_step_curve_hysteresis():
    c = StepCurve([180, 400, 650, 900], ["e", "t", "tt", "f"], 10)
    assert c.classify(400, None) == 1
    # boundary 1|2 at 525 ohm; 10 % margin -> needs >= 577.5
    assert c.classify(540, 1) == 1
    assert c.classify(580, 1) == 2
    assert c.classify(510, 2) == 2
    assert c.classify(470, 2) == 1


def run_pattern(dec, code, frames, t0=0.0, noise=False):
    t = t0
    for _ in range(frames):
        start = t
        while t < start + 4.0:
            phase = t - start
            idx, within = divmod(phase, 0.5)
            low = idx < code and within < 0.25
            if noise and abs(phase - 3.0) < 0.02:
                low = True  # 20 ms glitch
            dec.update(not low, t)
            t += 0.02
    return t


def test_pulse_decoder_all_codes():
    for code in range(1, 7):
        dec = PulseDecoder()
        run_pattern(dec, code, frames=1)
        assert dec.code is None, "one sequence is not enough"
        run_pattern(dec, code, frames=2, t0=4.0)
        assert dec.code == code


def test_pulse_decoder_ignores_glitches():
    dec = PulseDecoder()
    run_pattern(dec, 3, frames=3, noise=True)
    assert dec.code == 3


def test_pulse_decoder_rejects_long_pulse_and_clears():
    dec = PulseDecoder()
    t = run_pattern(dec, 2, frames=3)
    assert dec.code == 2
    for i in range(int(12 / 0.02)):
        dec.update(True, t + i * 0.02)  # idle high, no more codes
    assert dec.code is None
    dec2 = PulseDecoder()
    t = 0.0
    for _ in range(3):
        for i in range(200):
            dec2.update(not (i < 50), t)  # 1 s low pulse: invalid
            t += 0.02
    assert dec2.code is None
