# Testing Android Auto Disconnect and Reconnect

The repository does not currently ship an automated reconnect helper. Use the
service and Bluetooth commands below for a repeatable manual cycle. This keeps
the test independent of a particular Pi address, account name, phone MAC, or
checkout location.

## Prerequisites

- OpenAuto Prodigy installed as `openauto-prodigy.service`
- A phone already paired with the Pi
- Shell access to the Pi
- The Pi WiFi AP configured and running

Set the phone address once for the current shell. Obtain it with
`bluetoothctl devices`:

```bash
PHONE_MAC="AA:BB:CC:DD:EE:FF"  # replace with the phone's address
```

## Reconnect cycle

1. Follow the app log in a second terminal:

   ```bash
   journalctl -u openauto-prodigy.service -f
   ```

2. Disconnect the paired phone and stop the app:

   ```bash
   bluetoothctl disconnect "$PHONE_MAC"
   sudo systemctl stop openauto-prodigy.service
   ```

3. Confirm the app released the default AA listener. If
   `connection.tcp_port` is customized, substitute that value:

   ```bash
   ss -tlnp | grep 5277
   ```

   No output is expected while the app is stopped.

4. Leave healthy network and Bluetooth infrastructure running and start only
   the application service. That is the normal reconnect test:

   ```bash
   sudo systemctl start openauto-prodigy.service
   sudo systemctl status openauto-prodigy.service --no-pager
   ```

5. Reconnect from Android Auto on the phone. If the phone does not initiate the
   connection, request the existing Bluetooth connection from the Pi:

   ```bash
   bluetoothctl connect "$PHONE_MAC"
   ```

6. Verify the listener and session logs:

   ```bash
   ss -tlnp | grep 5277
   journalctl -u openauto-prodigy.service -n 200 --no-pager
   ```

Look for the TCP accept, protocol handshake, service discovery, and video
channel start. Repeat the cycle without deleting the pairing; clearing phone
state changes the scenario from reconnect testing to first-pair testing.

### Discovery startup retry

A transient RFCOMM listener failure does not require an application or
Bluetooth daemon restart. The application retries listener startup every two
seconds for up to 30 attempts. SDP registration begins only after RFCOMM owns a
nonzero channel and has its own retry timer with the same interval and budget.
Stopping the application cancels both retry paths; the next start receives a
fresh budget.

In the journal, an initial `RFCOMM listener failed, retrying` entry followed by
`RFCOMM listening on port` and `SDP service registered` is a recovered startup.
Only the terminal `failed after 30 attempts` error means the bounded recovery
was exhausted and the infrastructure checks below are needed.

## Failure isolation

- Run `sudo openauto-preflight --check-only` to check the WiFi radio, Wayland
  socket, and BlueZ SDP socket on a source install. The current prebuilt
  installer does not provide this helper; use the service and socket checks
  below instead.
- Check AP state with `systemctl status hostapd systemd-networkd --no-pager`.
- Check BlueZ state with `systemctl status bluetooth --no-pager` and
  `bluetoothctl show`.
- If the default port is still owned after stopping the service, inspect the
  process reported by `ss -tlnp`; do not immediately force-kill unrelated
  processes.

## Full-stack recovery

Use this only when the checks above show that infrastructure is unhealthy; it
is not part of the normal reconnect cycle. Restarting network services can
interrupt remote access. Restore the AP first when required, then restart
Bluetooth before the user audio stack so HFP profiles register in the expected
order, and start the app last:

```bash
sudo systemctl restart systemd-networkd.service hostapd.service  # only if AP is unhealthy
sudo systemctl restart bluetooth.service
systemctl --user restart pipewire pipewire-pulse wireplumber
sudo systemctl restart openauto-prodigy.service
```

After a BlueZ restart, confirm `/var/run/sdp` exists with the expected group
permissions before retrying AA discovery.
