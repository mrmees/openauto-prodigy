#!/usr/bin/env python3
"""Exercise the shared installer hardware and network contracts."""

from __future__ import annotations

import pathlib
import shlex
import subprocess
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
LIBRARY = REPO_ROOT / "config/installer/hardware-contracts.sh"
SOURCE_INSTALLER = REPO_ROOT / "install.sh"
PREBUILT_INSTALLER = REPO_ROOT / "install-prebuilt.sh"


FIVE_GHZ_FIXTURE = """\
Wiphy phy-test
\tBand 1:
\t\tCapabilities: 0x1020
\t\tFrequencies:
\t\t\t* 2412 MHz [1] (20.0 dBm)
\t\t\t* 2437 MHz [6] (20.0 dBm)
\tBand 2:
\t\tVHT Capabilities (0x338001b2):
\t\tFrequencies:
\t\t\t* 5180 MHz [36] (20.0 dBm)
\t\t\t* 5200 MHz [40] (disabled)
\t\t\t* 5220 MHz [44] (20.0 dBm) (no IR)
"""

FALLBACK_FIXTURE = """\
Wiphy phy-test
\tBand 1:
\t\tVHT Capabilities (0x00000000):
\t\tFrequencies:
\t\t\t* 2412 MHz [1] (20.0 dBm)
\t\t\t* 2437 MHz [6] (20.0 dBm)
\tBand 2:
\t\tFrequencies:
\t\t\t* 5180 MHz [36] (disabled)
\t\t\t* 5200 MHz [40] (20.0 dBm) (no IR)
"""

NO_USABLE_FIXTURE = """\
Wiphy phy-test
\tBand 1:
\t\tFrequencies:
\t\t\t* 2437 MHz [6] (disabled)
\tBand 2:
\t\tVHT Capabilities (0x338001b2):
\t\tFrequencies:
\t\t\t* 5180 MHz [36] (no IR)
"""

DFS_WITH_FALLBACK_FIXTURE = """\
Wiphy phy-test
\tBand 1:
\t\tFrequencies:
\t\t\t* 2437 MHz [6] (20.0 dBm)
\tBand 2:
\t\tVHT Capabilities (0x338001b2):
\t\tFrequencies:
\t\t\t* 5260 MHz [52] (20.0 dBm) (radar detection)
"""

DFS_ONLY_FIXTURE = """\
Wiphy phy-test
\tBand 1:
\t\tFrequencies:
\t\t\t* 2437 MHz [6] (disabled)
\tBand 2:
\t\tVHT Capabilities (0x338001b2):
\t\tFrequencies:
\t\t\t* 5260 MHz [52] (20.0 dBm) (radar detection)
"""


def run_bash(body: str, *, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["bash", "-c", body], cwd=REPO_ROOT, capture_output=True, text=True
    )
    if check and result.returncode != 0:
        raise AssertionError(
            f"shell harness failed ({result.returncode}):\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def extract_function(script: pathlib.Path, name: str) -> str:
    lines = script.read_text().splitlines()
    start = next(
        (index for index, line in enumerate(lines) if line == f"{name}() {{"), None
    )
    if start is None:
        raise AssertionError(f"{script.name} does not define {name}()")
    for end in range(start + 1, len(lines)):
        if lines[end] == "}":
            return "\n".join(lines[start : end + 1]) + "\n"
    raise AssertionError(f"{script.name} has an unterminated {name}()")


def test_input_properties_and_country() -> None:
    direct = ("2", "a", "12", "00000000000000000000000000000002", "  A\n")
    indirect = ("0", "1", "d", "10", "a d", "", "xyz", "2\n0")
    shell = f"""
set -euo pipefail
source {shlex.quote(str(LIBRARY))}
"""
    for value in direct:
        shell += f"oap_input_properties_have_direct {shlex.quote(value)}\n"
    for value in indirect:
        shell += (
            f"if oap_input_properties_have_direct {shlex.quote(value)}; then exit 20; fi\n"
        )
    shell += """
[[ "$(oap_normalize_country_code us)" == US ]]
[[ "$(oap_normalize_country_code Gb)" == GB ]]
for invalid in U USA U1 00 ' US' 'US ' 'éS' ''; do
    if oap_normalize_country_code "$invalid" >/dev/null; then exit 21; fi
done
"""
    run_bash(shell)


def make_probe_fixture(root: pathlib.Path, phy_info: str) -> tuple[pathlib.Path, pathlib.Path]:
    sys_root = root / "sys-class-net"
    phy_name = sys_root / "wlan-test/phy80211/name"
    phy_name.parent.mkdir(parents=True)
    phy_name.write_text("phy-test\n")
    fixture = root / "phy-info"
    fixture.write_text(phy_info)
    iw = root / "iw"
    iw.write_text(
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "[[ $# -eq 3 && $1 == phy && $2 == phy-test && $3 == info ]]\n"
        f"cat {shlex.quote(str(fixture))}\n"
    )
    iw.chmod(0o755)
    return sys_root, iw


def probe(phy_info: str) -> dict[str, str] | None:
    with tempfile.TemporaryDirectory(prefix="oap-hardware-probe-") as tmp:
        root = pathlib.Path(tmp)
        sys_root, iw = make_probe_fixture(root, phy_info)
        result = run_bash(
            f"""
set -euo pipefail
source {shlex.quote(str(LIBRARY))}
OAP_SYS_CLASS_NET_ROOT={shlex.quote(str(sys_root))}
OAP_IW_BIN={shlex.quote(str(iw))}
oap_probe_wifi_contract wlan-test
printf 'band=%s\nmode=%s\nchannel=%s\nvht=%s\n' \\
    "$OAP_WIFI_BAND" "$OAP_WIFI_HW_MODE" "$OAP_WIFI_CHANNEL" "$OAP_WIFI_USE_VHT"
""",
            check=False,
        )
        if result.returncode != 0:
            return None
        return dict(line.split("=", 1) for line in result.stdout.splitlines())


def test_wifi_probe_and_renderers() -> None:
    five = probe(FIVE_GHZ_FIXTURE)
    if five != {"band": "5", "mode": "a", "channel": "36", "vht": "true"}:
        raise AssertionError(f"unexpected 5 GHz contract: {five}")

    alternate_five = probe(
        FIVE_GHZ_FIXTURE.replace(
            "* 5180 MHz [36] (20.0 dBm)", "* 5180 MHz [36] (disabled)"
        ).replace("* 5200 MHz [40] (disabled)", "* 5200 MHz [40] (20.0 dBm)")
    )
    if alternate_five != {
        "band": "5",
        "mode": "a",
        "channel": "40",
        "vht": "true",
    }:
        raise AssertionError(f"unexpected alternate 5 GHz contract: {alternate_five}")

    five_without_vht = probe(
        FIVE_GHZ_FIXTURE.replace(
            "\t\tVHT Capabilities (0x338001b2):",
            "\t\tCapabilities: 0x338001b2",
            1,
        )
    )
    if five_without_vht != {
        "band": "5",
        "mode": "a",
        "channel": "36",
        "vht": "false",
    }:
        raise AssertionError(f"VHT leaked across iw bands: {five_without_vht}")

    fallback = probe(FALLBACK_FIXTURE)
    if fallback != {"band": "2.4", "mode": "g", "channel": "6", "vht": "false"}:
        raise AssertionError(f"unexpected 2.4 GHz contract: {fallback}")
    if probe(NO_USABLE_FIXTURE) is not None:
        raise AssertionError("probe accepted a fixture without a usable AP channel")

    dfs_fallback = probe(DFS_WITH_FALLBACK_FIXTURE)
    if dfs_fallback != {
        "band": "2.4",
        "mode": "g",
        "channel": "6",
        "vht": "false",
    }:
        raise AssertionError(f"DFS-only 5 GHz prevented fallback: {dfs_fallback}")
    if probe(DFS_ONLY_FIXTURE) is not None:
        raise AssertionError("probe accepted DFS-only channels without 802.11h")

    result = run_bash(
        f"""
set -euo pipefail
source {shlex.quote(str(LIBRARY))}
oap_select_wifi_contract {shlex.quote(FIVE_GHZ_FIXTURE)}
oap_render_networkd_config wlan-test 10.0.0.1
printf '%s\n' ---HOSTAPD---
oap_render_hostapd_config wlan-test Prodigy_test abcdefgh us
"""
    )
    network, hostapd = result.stdout.split("---HOSTAPD---\n", 1)
    for required in (
        "Name=wlan-test",
        "Address=10.0.0.1/24",
        "DHCPServer=yes",
        "ConfigureWithoutCarrier=yes",
    ):
        if required not in network.splitlines():
            raise AssertionError(f"networkd rendering is missing {required}")
    for required in (
        "interface=wlan-test",
        "ssid=Prodigy_test",
        "hw_mode=a",
        "channel=36",
        "ieee80211ac=1",
        "country_code=US",
        "wpa_passphrase=abcdefgh",
    ):
        if required not in hostapd.splitlines():
            raise AssertionError(f"hostapd rendering is missing {required}")

    fallback_render = run_bash(
        f"""
set -euo pipefail
source {shlex.quote(str(LIBRARY))}
oap_select_wifi_contract {shlex.quote(FALLBACK_FIXTURE)}
oap_render_hostapd_config wlan-test Prodigy_test abcdefgh US
"""
    ).stdout.splitlines()
    if "hw_mode=g" not in fallback_render or "channel=6" not in fallback_render:
        raise AssertionError("2.4 GHz fallback rendering does not use the selected channel")
    if "ieee80211ac=1" in fallback_render:
        raise AssertionError("2.4 GHz fallback rendering incorrectly enables VHT")


def run_network_installer(
    script: pathlib.Path, phy_info: str, country: str = "us"
) -> tuple[subprocess.CompletedProcess[str], bytes | None, bytes | None, list[str]]:
    with tempfile.TemporaryDirectory(prefix=f"oap-{script.stem}-network-") as tmp:
        root = pathlib.Path(tmp)
        sys_root, iw = make_probe_fixture(root, phy_info)
        network_out = root / "10-openauto-ap.network"
        hostapd_out = root / "hostapd.conf"
        actions = root / "actions"
        source = extract_function(script, "configure_network")
        shell = f"""
set -euo pipefail
source {shlex.quote(str(LIBRARY))}
OAP_SYS_CLASS_NET_ROOT={shlex.quote(str(sys_root))}
OAP_IW_BIN={shlex.quote(str(iw))}
WIFI_IFACE=wlan-test
WIFI_SSID=Prodigy_test
WIFI_PASS=abcdefgh
AP_IP=10.0.0.1
COUNTRY_CODE={shlex.quote(country)}
TEST_NETWORK={shlex.quote(str(network_out))}
TEST_HOSTAPD={shlex.quote(str(hostapd_out))}
TEST_ACTIONS={shlex.quote(str(actions))}
enter_interactive() {{ :; }}
leave_interactive() {{ :; }}
info() {{ :; }}
ok() {{ :; }}
warn() {{ :; }}
fail() {{ printf 'FAIL:%s\n' "$*" >&2; }}
configure_hostapd_lifecycle() {{ printf '%s\n' lifecycle >> "$TEST_ACTIONS"; }}
run_with_spinner() {{ shift; "$@"; }}
sudo() {{
    if [[ "$1" == "$OAP_IW_BIN" && "$2" == reg && "$3" == set ]]; then
        printf '%s\n' regulatory >> "$TEST_ACTIONS"
        return
    fi
    if [[ "$1" == install ]]; then
        printf '%s\n' managed-write >> "$TEST_ACTIONS"
        case "${{!#}}" in
            /etc/systemd/network/10-openauto-ap.network)
                /usr/bin/install -D -m "$4" "$5" "$TEST_NETWORK" ;;
            /etc/hostapd/hostapd.conf)
                /usr/bin/install -D -m "$4" "$5" "$TEST_HOSTAPD" ;;
            *) return 30 ;;
        esac
        return
    fi
    printf 'sudo:%s\n' "$*" >> "$TEST_ACTIONS"
}}
{source}
configure_network
[[ "$(stat -c %a "$TEST_NETWORK")" == 644 ]]
[[ "$(stat -c %a "$TEST_HOSTAPD")" == 644 ]]
"""
        result = run_bash(shell, check=False)
        network = network_out.read_bytes() if network_out.exists() else None
        hostapd = hostapd_out.read_bytes() if hostapd_out.exists() else None
        action_lines = actions.read_text().splitlines() if actions.exists() else []
        return result, network, hostapd, action_lines


def test_installer_integration_and_order() -> None:
    source_text = SOURCE_INSTALLER.read_text()
    prebuilt_text = PREBUILT_INSTALLER.read_text()
    source_main = extract_function(SOURCE_INSTALLER, "main")
    prebuilt_main = extract_function(PREBUILT_INSTALLER, "main")
    prebuilt_payload = extract_function(PREBUILT_INSTALLER, "require_payload")

    if source_main.index("load_hardware_contracts") < source_main.index(
        "resolve_source_checkout"
    ):
        raise AssertionError("source installer loads hardware contracts before checkout validation")
    if prebuilt_main.index("load_hardware_contracts") < prebuilt_main.index(
        "require_payload"
    ):
        raise AssertionError("prebuilt installer loads hardware contracts before payload validation")
    required_path = '"$PAYLOAD_DIR/config/installer/hardware-contracts.sh"'
    if required_path not in prebuilt_payload:
        raise AssertionError("prebuilt payload validation does not require the shared library")

    for script, text in (
        (SOURCE_INSTALLER, source_text),
        (PREBUILT_INSTALLER, prebuilt_text),
    ):
        hardware = extract_function(script, "setup_hardware")
        if "oap_input_properties_have_direct" not in hardware:
            raise AssertionError(f"{script.name} does not use the shared touch parser")
        network = extract_function(script, "configure_network")
        for call in (
            "oap_normalize_country_code",
            "oap_probe_wifi_contract",
            "oap_render_networkd_config",
            "oap_render_hostapd_config",
        ):
            if call not in network:
                raise AssertionError(f"{script.name} network path does not call {call}")
        if "(( $(cat" in text:
            raise AssertionError(f"{script.name} retains a decimal-only property parser")

    source_run = run_network_installer(SOURCE_INSTALLER, FIVE_GHZ_FIXTURE)
    prebuilt_run = run_network_installer(PREBUILT_INSTALLER, FIVE_GHZ_FIXTURE)
    for script, (result, network, hostapd, actions) in (
        (SOURCE_INSTALLER, source_run),
        (PREBUILT_INSTALLER, prebuilt_run),
    ):
        if result.returncode != 0:
            raise AssertionError(
                f"{script.name} network harness failed:\n{result.stderr}"
            )
        if actions[:4] != [
            "regulatory",
            "lifecycle",
            "managed-write",
            "managed-write",
        ]:
            raise AssertionError(f"{script.name} unexpected write order: {actions}")
        if network is None or hostapd is None:
            raise AssertionError(f"{script.name} did not install both managed configs")
    if source_run[1:3] != prebuilt_run[1:3]:
        raise AssertionError("install modes rendered different network configuration bytes")

    source_dfs_fallback = run_network_installer(
        SOURCE_INSTALLER, DFS_WITH_FALLBACK_FIXTURE
    )
    prebuilt_dfs_fallback = run_network_installer(
        PREBUILT_INSTALLER, DFS_WITH_FALLBACK_FIXTURE
    )
    if source_dfs_fallback[1:3] != prebuilt_dfs_fallback[1:3]:
        raise AssertionError("install modes diverged on the shared DFS fallback seam")
    dfs_hostapd = source_dfs_fallback[2]
    if dfs_hostapd is None:
        raise AssertionError("DFS fallback did not render hostapd configuration")
    dfs_lines = dfs_hostapd.decode().splitlines()
    if "hw_mode=g" not in dfs_lines or "channel=6" not in dfs_lines:
        raise AssertionError("installer DFS fallback did not select usable 2.4 GHz")
    if "ieee80211ac=1" in dfs_lines:
        raise AssertionError("installer DFS fallback incorrectly retained VHT")

    for script in (SOURCE_INSTALLER, PREBUILT_INSTALLER):
        result, network, hostapd, actions = run_network_installer(
            script, FIVE_GHZ_FIXTURE, country="USA"
        )
        if result.returncode == 0:
            raise AssertionError(f"{script.name} accepted an invalid country")
        if network is not None or hostapd is not None or actions:
            raise AssertionError(
                f"{script.name} mutated managed network state before country rejection"
            )

        result, network, hostapd, actions = run_network_installer(
            script, NO_USABLE_FIXTURE
        )
        if result.returncode == 0:
            raise AssertionError(f"{script.name} accepted no usable channel")
        if network is not None or hostapd is not None or actions != ["regulatory"]:
            raise AssertionError(
                f"{script.name} mutated managed network state before channel rejection"
            )


def main() -> int:
    test_input_properties_and_country()
    test_wifi_probe_and_renderers()
    test_installer_integration_and_order()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
