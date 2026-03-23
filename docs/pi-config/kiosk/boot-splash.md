# Boot Splash Configuration

## 1. Overview

The `rpi-splash-screen-support` package replaces the kernel's default four-raspberry logo with a custom TGA image displayed from early boot. The pre-converted TGA at `assets/splash.tga` is shipped in the repo — no ImageMagick is needed at install time.

This is a **one-time setup** — not self-healing across kernel updates. If the splash disappears after an `apt upgrade`, re-run `install.sh`.

**Trixie risk:** `rpi-splash-screen-support` was designed for Bookworm. Behavior on Trixie has MEDIUM confidence. See [raspberrypi/linux#7081](https://github.com/raspberrypi/linux/issues/7081) for the console text overlay issue — `loglevel=0` is the expected mitigation.

## 2. Prerequisites

- **Package:** `rpi-splash-screen-support` (from RPi apt repo, not Debian main)
- **TGA asset:** `assets/splash.tga` (pre-converted, shipped in repo)
- **Root access** for `configure-splash`, `systemctl`, and cmdline.txt edits

## 3. Deployment Steps

Phase 32's `configure_boot_splash()` function should translate these steps directly.

### Step 1: Install package

```bash
sudo apt-get install -y rpi-splash-screen-support
```

### Step 2: Deploy TGA to /lib/firmware/logo.tga

```bash
sudo cp assets/splash.tga /lib/firmware/logo.tga
```

This is the path that `configure-splash` and the initramfs hook expect.

### Step 3: Run configure-splash

```bash
sudo configure-splash /lib/firmware/logo.tga
```

This does three things:
1. Installs initramfs hook at `/etc/initramfs-tools/hooks/splash-screen-hook.sh`
2. Adds `fullscreen_logo=1 fullscreen_logo_name=logo.tga vt.global_cursor_default=0` to `/boot/firmware/cmdline.txt`
3. Runs `update-initramfs -u` to rebuild initramfs with the TGA

**WARNING:** `configure-splash` STRIPS these parameters from cmdline.txt: `quiet`, `console=tty1`, `plymouth.ignore-serial-consoles`. They must be re-added in the next step.

### Step 4: Repair cmdline.txt

After `configure-splash` mutates cmdline.txt, re-add the parameters it stripped and remove the Plymouth `splash` parameter:

```bash
# Read current cmdline.txt
CMDLINE=$(cat /boot/firmware/cmdline.txt)
# Remove Plymouth 'splash' parameter (we're replacing Plymouth with rpi-splash-screen-support)
CMDLINE=$(echo "$CMDLINE" | sed 's/ splash / /g; s/^splash //; s/ splash$//')
# Also remove plymouth.ignore-serial-consoles if present
CMDLINE=$(echo "$CMDLINE" | sed 's/ plymouth\.ignore-serial-consoles//g')
# Append quiet boot parameters (only if not already present)
for param in "quiet" "loglevel=0" "logo.nologo"; do
    if ! echo "$CMDLINE" | grep -q "$param"; then
        CMDLINE="$CMDLINE $param"
    fi
done
echo "$CMDLINE" | sudo tee /boot/firmware/cmdline.txt > /dev/null
```

**Full desired parameter set** on cmdline.txt (in addition to existing boot params):

| Parameter | Purpose |
|-----------|---------|
| `quiet` | Suppress kernel boot messages |
| `loglevel=0` | Suppress all kernel log messages (mitigation for #7081 console text overlay) |
| `logo.nologo` | Suppress the default four-raspberry logo (replaced by fullscreen_logo) |
| `vt.global_cursor_default=0` | Hide the blinking text cursor |
| `fullscreen_logo=1` | Enable fullscreen logo display (set by configure-splash) |
| `fullscreen_logo_name=logo.tga` | Specify the TGA filename (set by configure-splash) |

**Do NOT add `splash`** — that is a Plymouth parameter and we are masking Plymouth.

### Step 5: Mask Plymouth services

```bash
sudo systemctl mask plymouth-start.service
sudo systemctl mask plymouth-quit.service
sudo systemctl mask plymouth-quit-wait.service
```

Plymouth on Trixie has documented breakage. The static kernel splash via `rpi-splash-screen-support` is the replacement. This is a permanent commitment to non-Plymouth boot.

### Step 6: Rebuild initramfs (if not done by configure-splash)

```bash
sudo update-initramfs -u
```

This ensures the TGA is included in the current kernel's initramfs. `configure-splash` should have already done this, but running it again is safe and idempotent.

## 4. Verification Checklist

The installer should verify after setup:

```bash
# 1. Hook exists and is executable
test -x /etc/initramfs-tools/hooks/splash-screen-hook.sh && echo "OK: Hook exists"

# 2. TGA exists at firmware path
test -f /lib/firmware/logo.tga && echo "OK: TGA exists"

# 3. TGA is included in initramfs
lsinitramfs /boot/firmware/initrd.img-$(uname -r) | grep -q logo.tga && echo "OK: TGA in initramfs"

# 4. cmdline.txt has ALL required parameters
grep -q "fullscreen_logo=1" /boot/firmware/cmdline.txt && echo "OK: fullscreen_logo enabled"
grep -q "fullscreen_logo_name=logo.tga" /boot/firmware/cmdline.txt && echo "OK: fullscreen_logo_name set"
grep -q "quiet" /boot/firmware/cmdline.txt && echo "OK: quiet present"
grep -q "loglevel=0" /boot/firmware/cmdline.txt && echo "OK: loglevel=0 present"
grep -q "logo.nologo" /boot/firmware/cmdline.txt && echo "OK: logo.nologo present"
grep -q "vt.global_cursor_default=0" /boot/firmware/cmdline.txt && echo "OK: cursor hidden"

# 5. All 3 Plymouth services are masked
systemctl is-enabled plymouth-start.service 2>&1 | grep -q "masked" && echo "OK: plymouth-start masked"
systemctl is-enabled plymouth-quit.service 2>&1 | grep -q "masked" && echo "OK: plymouth-quit masked"
systemctl is-enabled plymouth-quit-wait.service 2>&1 | grep -q "masked" && echo "OK: plymouth-quit-wait masked"

# 6. Plymouth 'splash' parameter removed from cmdline.txt
! grep -qw "splash" /boot/firmware/cmdline.txt && echo "OK: Plymouth splash param removed"
```

## 5. Failure Policy

- Boot splash is **nice-to-have**, not load-bearing
- If any step fails, log a warning and continue installation
- The app still works; user just sees default boot screen instead of Prodigy logo
- Installer output: "Re-run install.sh to retry boot splash setup"

## 6. Durability and Known Risks

- **One-time setup** with no self-healing hooks
- Kernel updates trigger `update-initramfs -u` which re-runs hooks — splash survives IF the hook at `/etc/initramfs-tools/hooks/splash-screen-hook.sh` still exists and `/lib/firmware/logo.tga` is intact
- If splash disappears after `apt upgrade`: re-run `install.sh` or manually: `sudo configure-splash /lib/firmware/logo.tga && sudo update-initramfs -u`
- No initramfs hooks or dpkg triggers added — too complex for v0.7.0

## 7. TGA Regeneration

If the splash art changes, regenerate from the source PNG:

```bash
# ImageMagick 6 (dev VM, older systems):
convert assets/splash.png -flip -colors 224 -depth 8 -type TrueColor \
    -alpha off -compress none -define tga:bits-per-sample=8 assets/splash.tga

# ImageMagick 7 (Trixie, newer systems):
magick assets/splash.png -flip -colors 224 -depth 8 -type TrueColor \
    -alpha off -compress none -define tga:bits-per-sample=8 assets/splash.tga
```

Validate with:
```bash
file assets/splash.tga                  # Should say "Targa image data"
identify assets/splash.tga              # Should show 500x500
```

The TGA ships at native 500x500 resolution. The kernel framebuffer centers it on black. Since the background is already black, any borders are invisible on any display resolution.

## 8. Trixie Fallback

If `configure-splash` fails or produces unexpected results on Trixie, use this manual fallback (same result, no configure-splash tool):

```bash
# 1. Copy TGA manually
sudo cp assets/splash.tga /lib/firmware/logo.tga

# 2. Create initramfs hook manually
sudo tee /etc/initramfs-tools/hooks/splash-screen-hook.sh > /dev/null << 'HOOK'
#!/bin/sh
if [ -f /lib/firmware/logo.tga ]; then
    mkdir -p "${DESTDIR}/lib/firmware"
    cp /lib/firmware/logo.tga "${DESTDIR}/lib/firmware/logo.tga"
fi
HOOK
sudo chmod +x /etc/initramfs-tools/hooks/splash-screen-hook.sh

# 3. Set cmdline.txt parameters manually
# Add to /boot/firmware/cmdline.txt:
# quiet loglevel=0 logo.nologo vt.global_cursor_default=0 fullscreen_logo=1 fullscreen_logo_name=logo.tga
# Remove: splash, plymouth.ignore-serial-consoles

# 4. Mask Plymouth services
sudo systemctl mask plymouth-start.service
sudo systemctl mask plymouth-quit.service
sudo systemctl mask plymouth-quit-wait.service

# 5. Rebuild initramfs
sudo update-initramfs -u
```

This achieves the same result as `configure-splash` without depending on the tool.
