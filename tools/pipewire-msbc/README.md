# Patched-mSBC PipeWire build kit (A1b)

Builds a patched PipeWire **bluez5 SPA plugin** Debian package for the
Pi 4 head unit that **drops HFP LC3-SWB from the Bluetooth codec advertisement
(+BAC)** so the phone selects mSBC instead.

## Why

PipeWire's HFP **LC3-SWB** (Super Wide Band) *uplink* is silent at the far end
of a call — the phone hears nothing (bench 2026-07-05 and 2026-07-11). mSBC
uplink works. See design **§A1b**.

The fix (`0001-hfp-disable-lc3-swb.patch`) makes `device_supports_codec()` in
`spa/plugins/bluez5/backend-native.c` return `false` for
`HFP_AUDIO_CODEC_LC3_SWB`. With SWB no longer "supported", it is dropped from
the `+BAC` advertisement the Pi (HFP Hands-Free) sends, so the Audio Gateway
(the phone) falls back to mSBC. CVSD and mSBC remain advertised, so wide-band
call audio still works — just via mSBC, not LC3-SWB.

This is a **temporary** workaround. Remove the patch once upstream fixes SWB
uplink encode.

## What gets built

One package: `out/libspa-0.2-bluetooth_<ver>+prodigy<n>_arm64.deb` — the
patched bluez5 plugin (e.g.
`libspa-0.2-bluetooth_1.4.2-1+rpt3+prodigy1_arm64.deb`).

Packaging facts (learned the hard way):

- The binary package is **`libspa-0.2-bluetooth`** — there is no
  "libspa-0.2-bluez5" package in Trixie (only the upstream source dir is
  called bluez5).
- The Pi runs **Raspberry Pi OS**, whose pipewire is a `+rptN` rebuild from
  `archive.raspberrypi.com` (e.g. `1.4.2-1+rpt3`) — not stock Debian
  `1.4.2-1`. The build sources from the RPi archive so the result matches the
  installed base exactly, and `+rpt3+prodigy1` sorts as an **upgrade** over
  the installed `+rpt3` (no `--allow-downgrades` needed).
- **Dependency pinning:** stock packaging gives `libspa-0.2-bluetooth` a
  strict `Depends: libspa-0.2-modules (= <its own version>)`. Shipping a
  `+prodigy` libspa-0.2-modules to satisfy that would break the *other* stock
  packages that pin modules to the base version (`libpipewire-0.3-0t64`,
  `libspa-0.2-libcamera`) — apt then proposes removing the whole audio stack.
  Instead, build.sh pins our package's dependency to the **stock base
  version** (`libspa-0.2-modules (= 1.4.2-1+rpt3)`), so the single deb
  installs against the untouched stack. Side effect (desirable): if the Pi
  later upgrades pipewire, the held package's dependency breaks **loudly**
  instead of silently mixing plugin/module ABIs — rebuild at that point.

## Build

```bash
./build.sh
```

- Builds inside an **emulated arm64** Debian Trixie container (qemu binfmt),
  so it is **slow** (an hour-ish) — that's expected; let it run.
- `build.sh` self-registers the qemu arm64 binfmt handler if it isn't already
  (needs Docker + one `--privileged` helper run). The repo's `cross-build.sh`
  uses a *native* cross-compiler and does not set this up, hence the guard.
- Trust for the RPi archive is bootstrapped from the
  `raspberrypi-archive-keyring` package fetched over HTTPS — the archive's
  standalone `.gpg.key` fails Trixie's OpenPGP policy (SHA1-bound signature
  rejected by sqv).
- Output lands in `out/` (gitignored).

## Rebuild when the archive bumps pipewire

Just **rerun `./build.sh`**. It re-fetches the current source (highest version
wins — the RPi `+rptN` rebuild) and re-applies the patch. If the pipewire
version bumped, the patch's hunk offsets may drift; `quilt push` will fuzz or
fail. If it fails:

1. `apt-get source pipewire` in a Trixie container (build.sh shows the repo
   setup),
2. inspect `spa/plugins/bluez5/backend-native.c` around the
   `HFP_AUDIO_CODEC_LC3_SWB` case in `device_supports_codec()`,
3. regenerate `0001-hfp-disable-lc3-swb.patch` (same semantic change: the
   SWB case becomes an unconditional `return false;`).

If the keyring download 404s, the archive rotated its keyring package — pick
the newest from
<https://archive.raspberrypi.com/debian/pool/main/r/raspberrypi-archive-keyring/>
and update `KEYRING_DEB_URL` in `build.sh`.

## Install on the Pi

The staged deb lives in `~/pipewire-msbc/` on the Pi. **Its version must be
based on the Pi's installed pipewire version** (e.g. `1.4.2-1+rpt3` →
`1.4.2-1+rpt3+prodigy1`); if the Pi upgraded pipewire since the build, rebuild
first. Install and hold it so `apt upgrade` doesn't replace it:

```bash
cd ~/pipewire-msbc
sudo apt install ./libspa-0.2-bluetooth_*+prodigy*_arm64.deb
sudo apt-mark hold libspa-0.2-bluetooth
```

Sanity check first (optional): `apt-get -s install ./libspa-0.2-bluetooth_*+prodigy*_arm64.deb`
must show exactly one `Inst` line and **no removals**.

Restart audio to load the new plugin (or reboot):

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
```

Verify: place a call; the HFP transport should negotiate mSBC, and the far
end should hear the mic.

## Revert to the stock package

```bash
sudo apt-mark unhold libspa-0.2-bluetooth
sudo apt install --allow-downgrades libspa-0.2-bluetooth=1.4.2-1+rpt3
```

(`--allow-downgrades` because the stock `+rptN` version sorts below our
`+rptN+prodigyN`; adjust the version if the archive has moved on — plain
`sudo apt install libspa-0.2-bluetooth` then suffices.) Then restart PipeWire
(or reboot) as above.
