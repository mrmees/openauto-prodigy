# Kiosk Session Configuration Files

Reference configuration files for the OpenAuto Prodigy kiosk session. These files are deployed to system paths by the installer (Phase 32). During development, they can be deployed manually for testing.

## File Inventory

| Reference File | Deploys To | Purpose |
|---------------|------------|---------|
| `openauto-kiosk.desktop` | `/usr/share/wayland-sessions/openauto-kiosk.desktop` | XDG session entry -- LightDM uses this to start the kiosk session |
| `labwc/rc.xml` | `/etc/openauto-kiosk/labwc/rc.xml` | Stripped labwc config: no decorations, no desktop chrome, multi-touch safe |
| `labwc/autostart` | `/etc/openauto-kiosk/labwc/autostart` | Kiosk autostart script (splash added in Phase 29; app launches via systemd) |
| `labwc/environment` | `/etc/openauto-kiosk/labwc/environment` | Environment variables for the kiosk compositor session |
| `50-openauto-kiosk.conf` | `/etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf` | LightDM drop-in for autologin into kiosk session |
| `accountsservice-user` | `/var/lib/AccountsService/users/$USER` | AccountsService session preference (belt-and-suspenders with LightDM) |

## How It Works

The kiosk session uses labwc's `-C` flag to load config from `/etc/openauto-kiosk/labwc/` instead of the user's `~/.config/labwc/`. This keeps the kiosk config completely separate from the normal desktop session config -- switching between kiosk and desktop does not affect either session's configuration.

**Boot sequence:**
1. LightDM reads `50-openauto-kiosk.conf` and autologins the user into the `openauto-kiosk` session
2. LightDM finds `openauto-kiosk.desktop` in `/usr/share/wayland-sessions/` and runs its `Exec` line
3. labwc starts with the kiosk config directory (`-C /etc/openauto-kiosk/labwc`)
4. labwc runs `autostart` from the kiosk config directory (splash in Phase 29)
5. `graphical.target` is reached, systemd starts `openauto-prodigy.service`
6. The app renders fullscreen (via the `windowRule` in `rc.xml`)

## Manual Deployment (Development/Testing)

Deploy the files to their system paths:

```bash
# Create kiosk labwc config directory
sudo mkdir -p /etc/openauto-kiosk/labwc

# Deploy labwc config files
sudo cp labwc/rc.xml /etc/openauto-kiosk/labwc/rc.xml
sudo cp labwc/autostart /etc/openauto-kiosk/labwc/autostart
sudo cp labwc/environment /etc/openauto-kiosk/labwc/environment

# Deploy XDG session file
sudo cp openauto-kiosk.desktop /usr/share/wayland-sessions/openauto-kiosk.desktop

# Deploy LightDM drop-in
sudo mkdir -p /etc/lightdm/lightdm.conf.d
sudo cp 50-openauto-kiosk.conf /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf

# Deploy AccountsService session entry
# Replace $USER with the actual username (e.g., matt)
sudo cp accountsservice-user /var/lib/AccountsService/users/$USER
```

**Test the session** (without rebooting):
```bash
# Switch to the kiosk session from a desktop terminal or SSH
dm-tool switch-to-user matt openauto-kiosk
```

## AccountsService Entry

The file at `/var/lib/AccountsService/users/$USER` tells LightDM (and other display managers) which session to use for autologin. This is a belt-and-suspenders approach alongside the LightDM drop-in:

- The **LightDM drop-in** (`50-openauto-kiosk.conf`) is the primary mechanism and survives raspi-config changes
- The **AccountsService entry** is a secondary mechanism that some LightDM configurations prefer

Both should agree on the session name (`openauto-kiosk`).

## Reverting to Desktop Session

To switch back to the standard RPi OS desktop:

```bash
# Option 1: Remove the LightDM drop-in (cleanest)
sudo rm /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf
sudo reboot

# Option 2: Update AccountsService too (belt-and-suspenders)
sudo rm /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf
sudo sed -i 's/Session=openauto-kiosk/Session=rpd-labwc/' /var/lib/AccountsService/users/$USER
sudo reboot
```

## Warning: raspi-config and Session Selection

**Do not use `raspi-config` (System Options > Boot / Auto Login) to change boot settings after installing kiosk mode.** raspi-config directly modifies `/etc/lightdm/lightdm.conf` and `/var/lib/AccountsService/users/$USER`, overwriting the kiosk session selection with `rpd-labwc`.

The LightDM drop-in file (`50-openauto-kiosk.conf`) in `/etc/lightdm/lightdm.conf.d/` takes precedence over the main `lightdm.conf`, so the kiosk session will survive raspi-config changes to the main config. However, raspi-config **will** overwrite the AccountsService entry, which may cause inconsistencies.

If you accidentally run raspi-config and lose kiosk mode, re-deploy the AccountsService entry:
```bash
echo -e "[User]\nSession=openauto-kiosk" | sudo tee /var/lib/AccountsService/users/$USER
sudo reboot
```
