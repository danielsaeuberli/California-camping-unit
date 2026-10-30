import pytest
import yaml

from camperd.hal.base import HalError
from camperd.hal.hw import HwHal
from camperd.hal.sm_h001 import CardError, SmH001
from camperd.hal.sm_i009 import SmI009
from camperd.profile import Profile
from tests.conftest import PROFILE
from tests.fake_sm_h001 import FakeBus, FakeCard
from tests.fake_sm_i009 import FakeGpio, FakeInputCard

H001 = 0x28  # stack 0
I009 = 0x27  # (0x20 + 0) ^ 7


@pytest.fixture
def cards():
    return {H001: FakeCard(), I009: FakeInputCard()}


@pytest.fixture
def bus(cards):
    return FakeBus(cards)


def profile_with(**ladder):
    data = yaml.safe_load(PROFILE.read_text())
    data["outputs"]["heater_ladder"].update(ladder)
    return Profile(data)


async def make_hal(profile, bus):
    gpio = FakeGpio()
    hal = HwHal(profile, bus=bus, lines=gpio)
    await hal.start()
    return hal, gpio


# ----- SM-H-001 -----
def test_relay_single_channel_and_verify(bus, cards):
    card = SmH001(bus, 0)
    card.set_relay(3, True)
    assert cards[H001].relays == 0b100
    assert (H001, 0x01, 3) in bus.writes  # SET register with channel number
    card.set_relay(3, False)
    assert cards[H001].relays == 0


def test_stuck_relay_detected(bus, cards):
    cards[H001].relays = 0b1
    cards[H001].stuck_relays = 0b1
    with pytest.raises(CardError, match="did not switch off"):
        SmH001(bus, 0).set_relay(1, False)


def test_adc_and_rail(bus, cards):
    cards[H001].adc_mv = [100, 200, 300, 400, 500, 600, 700, 3000]
    card = SmH001(bus, 0)
    assert card.read_adc_v() == [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 3.0]
    assert card.read_rail_v() == 3.3


# ----- SM-I-009 -----
def test_input_address_and_bit_order(bus, cards):
    card = SmI009(bus, 0)
    assert card.addr == 0x27
    cards[I009].inputs[0] = True  # input 1 = port bit 15, active low
    cards[I009].inputs[9] = True
    di = card.read_inputs()
    assert di[0] and di[9] and sum(di) == 2


def test_v2_card_has_no_counters(bus, cards):
    cards[I009].v3 = False
    card = SmI009(bus, 0)
    assert card.probe() is None
    assert not card.has_counters
    with pytest.raises(CardError, match="V3"):
        card.enable_edge_count(6)


# ----- HAL -----
async def test_start_probes_clears_default_and_enables_speed_counter(profile, bus, cards):
    cards[H001].relay_default = 0b1111  # S1: card would switch relays on at power-up
    cards[H001].relays = 0xFF
    hal, gpio = await make_hal(profile, bus)
    assert cards[H001].relay_default == 0 and cards[H001].relays == 0
    assert cards[I009].rising & 0b100000 and cards[I009].falling & 0b100000  # DI6, both edges
    assert gpio.levels[26] is True  # ALIVE
    await hal.stop()
    assert gpio.levels[26] is False


@pytest.mark.parametrize("missing,name", [(H001, "SM-H-001"), (I009, "SM-I-009")])
async def test_missing_card_fails_start(profile, cards, missing, name):
    del cards[missing]
    with pytest.raises(HalError, match=name):
        await make_hal(profile, FakeBus(cards))


async def test_io_mapping(profile, bus, cards):
    hal, _ = await make_hal(profile, bus)
    cards[I009].inputs[9] = True  # DI10 = ignition
    di = await hal.read_di()
    assert di[9] and sum(di) == 1
    await hal.write_relay(4, True)
    assert cards[H001].relays == 0b1000
    cards[H001].adc_mv[7] = 2500
    assert (await hal.read_ai())[7] == 2.5
    cards[I009].counts[5] = 4242
    assert await hal.read_counters((6,)) == {6: 4242}
    with pytest.raises(ValueError):
        await hal.write_relay(9, True)


async def test_ladder_polarity_unknown_never_energises(profile, bus):
    hal, gpio = await make_hal(profile, bus)  # gpio_active: TODO_CHECK
    assert 17 not in gpio.levels  # ladder lines not driven, keep config.txt boot level
    await hal.write_gpio(1, False)
    with pytest.raises(HalError, match="polarity"):
        await hal.write_gpio(1, True)


@pytest.mark.parametrize("active,on_level", [("low", False), ("high", True)])
async def test_ladder_polarity(bus, active, on_level):
    hal, gpio = await make_hal(profile_with(gpio_active=active), bus)
    assert gpio.levels[17] is (not on_level)  # off at start
    await hal.write_gpio(1, True)
    assert gpio.levels[17] is on_level
    hal.all_off_sync()
    assert gpio.levels[17] is (not on_level)


async def test_all_off_even_with_bus_lock_held(profile, bus, cards):
    hal, _ = await make_hal(profile, bus)
    cards[H001].relays = 0xFF
    hal._lock.acquire()  # a hung bus call holds the lock
    hal.all_off_sync()
    assert cards[H001].relays == 0


async def test_bus_failure_raises_hal_error(profile, bus):
    hal, _ = await make_hal(profile, bus)
    bus.fail = True
    with pytest.raises(HalError):
        await hal.read_di()
    with pytest.raises(HalError):
        await hal.write_relay(1, True)


async def test_s5_controller_blocks_roof_on_input_card_failure(sim_profile, bus, clock):
    from camperd.core import Controller
    from camperd.modules.energy import VictronStore

    hal, _ = await make_hal(sim_profile, bus)
    ctrl = Controller(sim_profile, hal, clock, VictronStore(clock))
    await ctrl.init_outputs()
    await ctrl.cycle()
    assert ctrl.di_ok
    bus.fail = True
    for _ in range(3):
        clock.advance(0.02)
        await ctrl.cycle()
    assert not ctrl.di_ok
    assert ctrl.roof_hold("open", 1) == "blocked_io"
