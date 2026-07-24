# Current Milestone Validation

These checks target ALPHA-26-07-24-01. They are neither confirmed backlog nor
implementation scope. Run them on the installed Pi, record the evidence, then
delete a passing check or promote a failing one into engineering backlog with
an exact reproduction.

- **Clean boot timing** — measure power-on to responsive application and record
  the critical systemd chain. Source install disables both network wait-online
  services, while install-prebuilt.sh has no equivalent action at audit base
  bbce99a; determine whether the official prebuilt path still waits on an
  unused network manager.

- **Bluetooth advertising after disconnect** — connect and disconnect one
  paired phone, then verify a new second phone can discover the head unit
  without restarting Prodigy or Bluetooth. Capture bluetoothctl state and the
  app/BlueZ journal if discovery fails.

- **Startup D-Bus marshalling warnings** — capture one clean-boot application
  journal and check for the former read-only QDBusArgument and unregistered
  MediaPlayer1 Track-type warnings. PR #33 substantially rewrote Bluetooth
  demarshalling, so old observations are not sufficient evidence.
