import pytest

from camperd.hal.base import HalError
from camperd.hal.hw import HwHal
from camperd.hal.sm_h001 import CardError, SmH001
from tests.fake_sm_h001 import FakeBus, FakeCard


class FakeAlive:
    def __init__(self):
        self.on = None

    def set(self, on):
        self.on = on


@pytest.fixture
def cards():
    return {0x28: FakeCard(), 0x29: FakeCard()}


@pytest.fixture
def bus(cards):
    return FakeBus(cards)


async def make_hal(profile, bus):
    alive = FakeAlive()
    hal = HwHal(profile, bus=bus, alive=alive)
    await hal.start()
    return hal, alive


def test_card_relay_single_channel_and_verify(bus, cards):
    card = SmH001(bus, 0)
    card.set_relay(3, True)
    assert cards[0x28].relays == 0b100
    assert (0x28, 0x01, 3) in bus.writes  # SET register with channel number
    card.set_relay(3, False)
    assert cards[0x28].relays == 0


def test_stuck_relay_detected(bus, cards):
    cards[0x28].relays = 0b1
    cards[0x28].stuck_relays = 0b1
    with pytest.raises(CardError, match="did not switch off"):
        SmH001(bus, 0).set_relay(1, False)


def test_adc_and_rail(bus, cards):
    cards[0x28].adc_mv = [100, 200, 300, 400, 500, 600, 700, 3000]
    card = SmH001(bus, 0)
    assert card.read_adc_v() == [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 3.0]
    assert card.read_rail_v() == 3.3


async def test_start_probes_and_clears_power_up_default(profile, bus, cards):
    cards[0x28].relay_default = 0b1111  # S1: card would switch relays on at power-up
    cards[0x29].relays = 0xFF
    hal, alive = await make_hal(profile, bus)
    assert cards[0x28].relay_default == 0
    assert cards[0x29].relays == 0
    assert alive.on is True
    # speed DI6 = card A opto 6, both edges counted
    assert cards[0x28].rising & 0b100000 and cards[0x28].falling & 0b100000
    await hal.stop()
    assert alive.on is False


async def test_missing_card_fails_start(profile, cards):
    del cards[0x29]
    with pytest.raises(HalError, match="sm_h001_b"):
        await make_hal(profile, FakeBus(cards))


async def test_channel_mapping_across_cards(profile, bus, cards):
    hal, _ = await make_hal(profile, bus)
    cards[0x28].opto = 0b0000_0001
    cards[0x29].opto = 0b0000_0010  # DI10 = ignition
    di = await hal.read_di()
    assert di[0] and di[9] and sum(di) == 2
    await hal.write_relay(10, True)  # K10 = card B relay 2
    assert cards[0x29].relays == 0b10 and cards[0x28].relays == 0
    cards[0x29].adc_mv[0] = 1234
    ai = await hal.read_ai()
    assert len(ai) == 16 and ai[8] == 1.234
    cards[0x28].counts[5] = 4242
    assert await hal.read_counters((6,)) == {6: 4242}


async def test_all_off_even_with_bus_lock_held(profile, bus, cards):
    hal, _ = await make_hal(profile, bus)
    cards[0x28].relays = 0xFF
    cards[0x29].relays = 0x0F
    hal._lock.acquire()  # a hung bus call holds the lock
    hal.all_off_sync()
    assert cards[0x28].relays == 0 and cards[0x29].relays == 0


async def test_bus_failure_raises_hal_error(profile, bus):
    hal, _ = await make_hal(profile, bus)
    bus.fail = True
    with pytest.raises(HalError):
        await hal.read_di()
    with pytest.raises(HalError):
        await hal.write_relay(1, True)


async def test_s5_controller_blocks_roof_on_card_failure(sim_profile, bus, cards, clock):
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


def test_same_stack_twice_rejected(profile):
    import yaml

    from camperd.profile import Profile, ProfileError
    from tests.conftest import PROFILE

    data = yaml.safe_load(PROFILE.read_text())
    data["buses"]["sm_h001_b"]["stack"] = 0
    with pytest.raises(ProfileError, match="different stack"):
        HwHal(Profile(data), bus=FakeBus({}), alive=FakeAlive())
