import unittest

from openauto_system import parse_set_proxy_route_request


class OpenAutoSystemRouteValidationTests(unittest.TestCase):
    def test_disable_request_normalizes(self):
        result = parse_set_proxy_route_request({"active": False})
        self.assertEqual(result, {"active": False})

    def test_active_request_parses_and_defaults_user(self):
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertTrue(result["active"])
        self.assertEqual(result["host"], "10.0.0.10")
        self.assertEqual(result["port"], 1080)
        self.assertEqual(result["user"], "oap")
        self.assertEqual(result["skip_interfaces"], ["eth0", "lo"])
        self.assertEqual(result["skip_tcp_ports"], [22])
        self.assertEqual(result["skip_networks"], [])

    def test_active_request_parses_custom_route_exceptions(self):
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "user": "oap",
            "password": "deadbeef",
            "skip_interfaces": ["wlan0"],
            "skip_tcp_ports": [22, 443],
            "skip_networks": ["172.16.0.0/12", "192.168.0.0/16"],
        })
        self.assertEqual(result["skip_interfaces"], ["wlan0"])
        self.assertEqual(result["skip_tcp_ports"], [22, 443])
        self.assertEqual(result["skip_networks"], ["172.16.0.0/12", "192.168.0.0/16"])

    def test_missing_host_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "port": 1080,
                "password": "deadbeef",
            })

    def test_missing_password_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
            })

    def test_invalid_port_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 70000,
                "password": "deadbeef",
            })

    def test_invalid_active_type_raises(self):
        with self.assertRaises(TypeError):
            parse_set_proxy_route_request({
                "active": 1,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
            })

    def test_invalid_skip_interfaces_rejected(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
                "skip_interfaces": "eth0",
            })

    def test_invalid_skip_tcp_ports_rejected(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
                "skip_tcp_ports": [0],
            })
