import pytest

from test_library.port_range import LackOfPortsError, PortRange


@pytest.fixture
def port_range():
    return PortRange(1000, 1100)


def test_getting_unique_single_ports(port_range):
    first = port_range.allocate_port()
    second = port_range.allocate_port()

    assert first != second


def test_getting_unique_port_ranges(port_range):
    first = port_range.allocate_port_range(50)
    second = port_range.allocate_port_range(50)

    assert first.end <= second.begin or second.end <= first.begin


def test_reporting_error_when_there_are_no_available_ports(port_range):
    # Allocate all ports
    port_range.allocate_port_range(100)

    with pytest.raises(LackOfPortsError):
        port_range.allocate_port()

    with pytest.raises(LackOfPortsError):
        port_range.allocate_port_range(1)
