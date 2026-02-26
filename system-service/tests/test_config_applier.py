import os
import pytest
import yaml
from config_applier import ConfigApplier, ConfigValidationError


@pytest.fixture
def config_dir(tmp_path):
    """Create a temp config.yaml."""
    config = {
        "connection": {
            "wifi_ap": {
                "ssid": "TestAP",
                "password": "testpass123",
                "channel": 36,
                "band": "a",
                "interface": "wlan0",
            },
            "tcp_port": 5277,
        },
        "network": {
            "ap_ip": "10.0.0.1",
            "dhcp_range_start": 10,
            "dhcp_range_end": 50,
        },
    }
    config_path = tmp_path / "config.yaml"
    config_path.write_text(yaml.dump(config))
    return tmp_path, str(config_path)


@pytest.fixture
def applier(config_dir, tmp_path):
    config_tmp, config_path = config_dir
    output_dir = tmp_path / "output"
    output_dir.mkdir()
    return ConfigApplier(
        config_path=config_path,
        hostapd_path=str(output_dir / "hostapd.conf"),
        networkd_path=str(output_dir / "10-openauto-ap.network"),
    )


def test_apply_wifi_writes_hostapd(applier):
    result = applier.apply_section("wifi")
    assert result["ok"] is True
    content = open(applier._hostapd_path).read()
    assert "ssid=TestAP" in content
    assert "wpa_passphrase=testpass123" in content
    assert "channel=36" in content
    assert "hw_mode=a" in content


def test_apply_network_writes_networkd(applier):
    result = applier.apply_section("network")
    assert result["ok"] is True
    content = open(applier._networkd_path).read()
    assert "Address=10.0.0.1/24" in content
    assert "PoolOffset=10" in content
    assert "PoolSize=40" in content


def test_validate_ssid_length():
    with pytest.raises(ConfigValidationError, match="SSID"):
        ConfigApplier._validate_wifi({"ssid": "", "password": "testpass1", "channel": 36, "band": "a"})
    with pytest.raises(ConfigValidationError, match="SSID"):
        ConfigApplier._validate_wifi({"ssid": "x" * 33, "password": "testpass1", "channel": 36, "band": "a"})


def test_validate_password_length():
    with pytest.raises(ConfigValidationError, match="password"):
        ConfigApplier._validate_wifi({"ssid": "Test", "password": "short", "channel": 36, "band": "a"})


def test_validate_ip_address():
    with pytest.raises(ConfigValidationError, match="IP"):
        ConfigApplier._validate_network({"ap_ip": "not.an.ip", "dhcp_range_start": 10, "dhcp_range_end": 50})


def test_unknown_section_returns_error(applier):
    result = applier.apply_section("nonexistent")
    assert result["ok"] is False
    assert "Unknown" in result["error"]
