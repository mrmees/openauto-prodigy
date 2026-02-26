import ipaddress
import logging
import os
import tempfile
import yaml

log = logging.getLogger(__name__)

TEMPLATE_DIR = os.path.join(os.path.dirname(__file__), "templates")


class ConfigValidationError(Exception):
    pass


class ConfigApplier:
    def __init__(
        self,
        config_path: str,
        hostapd_path: str = "/etc/hostapd/hostapd.conf",
        networkd_path: str = "/etc/systemd/network/10-openauto-ap.network",
        country_code: str = "US",
    ):
        self._config_path = config_path
        self._hostapd_path = hostapd_path
        self._networkd_path = networkd_path
        self._country_code = country_code

    def _read_config(self) -> dict:
        with open(self._config_path, "r") as f:
            return yaml.safe_load(f) or {}

    def apply_section(self, section: str) -> dict:
        try:
            config = self._read_config()
            if section == "wifi":
                return self._apply_wifi(config)
            elif section == "network":
                return self._apply_network(config)
            else:
                return {"ok": False, "error": f"Unknown section: {section}"}
        except ConfigValidationError as e:
            return {"ok": False, "error": str(e)}
        except Exception as e:
            log.exception("Failed to apply config section %s", section)
            return {"ok": False, "error": str(e)}

    def _apply_wifi(self, config: dict) -> dict:
        wifi = config.get("connection", {}).get("wifi_ap", {})
        self._validate_wifi(wifi)

        template = self._read_template("hostapd.conf.tmpl")
        content = template.format(
            interface=wifi.get("interface", "wlan0"),
            ssid=wifi["ssid"],
            password=wifi["password"],
            channel=wifi.get("channel", 36),
            band=wifi.get("band", "a"),
            country_code=self._country_code,
        )
        self._atomic_write(self._hostapd_path, content)
        return {"ok": True, "restarted": ["hostapd"]}

    def _apply_network(self, config: dict) -> dict:
        net = config.get("network", {})
        wifi = config.get("connection", {}).get("wifi_ap", {})
        self._validate_network(net)

        pool_start = net.get("dhcp_range_start", 10)
        pool_end = net.get("dhcp_range_end", 50)

        template = self._read_template("10-openauto-ap.network.tmpl")
        content = template.format(
            interface=wifi.get("interface", "wlan0"),
            ap_ip=net.get("ap_ip", "10.0.0.1"),
            dhcp_range_start=pool_start,
            dhcp_pool_size=pool_end - pool_start,
        )
        self._atomic_write(self._networkd_path, content)
        return {"ok": True, "restarted": ["systemd-networkd"]}

    @staticmethod
    def _validate_wifi(wifi: dict):
        ssid = wifi.get("ssid", "")
        if not ssid or len(ssid) > 32:
            raise ConfigValidationError("SSID must be 1-32 characters")
        password = wifi.get("password", "")
        if len(password) < 8:
            raise ConfigValidationError("WPA2 password must be at least 8 characters")

    @staticmethod
    def _validate_network(net: dict):
        try:
            ipaddress.IPv4Address(net.get("ap_ip", ""))
        except (ipaddress.AddressValueError, ValueError):
            raise ConfigValidationError(f"Invalid IP address: {net.get('ap_ip')}")

    def _read_template(self, name: str) -> str:
        path = os.path.join(TEMPLATE_DIR, name)
        with open(path, "r") as f:
            return f.read()

    @staticmethod
    def _atomic_write(path: str, content: str):
        dir_name = os.path.dirname(path)
        fd, tmp_path = tempfile.mkstemp(dir=dir_name, suffix=".tmp")
        try:
            os.write(fd, content.encode())
            os.close(fd)
            os.rename(tmp_path, path)
        except Exception:
            os.close(fd)
            os.unlink(tmp_path)
            raise
