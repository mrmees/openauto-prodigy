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
