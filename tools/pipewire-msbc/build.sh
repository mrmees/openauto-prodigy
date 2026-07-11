#!/usr/bin/env bash
# Build a patched PipeWire bluez5 SPA plugin (.deb) for arm64 (Raspberry Pi 4,
# Raspberry Pi OS Trixie) with HFP LC3-SWB dropped from the +BAC advertisement
# so the phone selects mSBC. See README.md and design §A1b for the why.
#
# Output (single deb — see dependency-pinning note below):
#   out/libspa-0.2-bluetooth_<ver>+prodigy<n>_arm64.deb
#   e.g. libspa-0.2-bluetooth_1.4.2-1+rpt3+prodigy1_arm64.deb
#
# Notes:
# - The Debian binary package for the bluez5 SPA plugin is named
#   libspa-0.2-bluetooth (the upstream source dir is spa/plugins/bluez5;
#   no "libspa-0.2-bluez5" package exists in Trixie).
# - The Pi runs Raspberry Pi OS, whose pipewire is a +rptN rebuild from
#   archive.raspberrypi.com (e.g. 1.4.2-1+rpt3) — NOT stock Debian 1.4.2-1.
#   We build from the RPi archive source so the result matches the installed
#   base exactly and versions as an upgrade (+rpt3+prodigy1 > +rpt3).
# - The RPi archive's standalone .gpg.key fails Trixie's OpenPGP policy
#   (SHA1-bound signature rejected by sqv), so we bootstrap trust from the
#   raspberrypi-archive-keyring package fetched over HTTPS instead.
# - Dependency pinning: stock packaging gives libspa-0.2-bluetooth a strict
#   Depends: libspa-0.2-modules (= ${binary:Version}). Shipping a +prodigy
#   libspa-0.2-modules would break the OTHER stock packages that pin
#   modules (= base) — libpipewire-0.3-0t64, libspa-0.2-libcamera — and apt
#   then wants to remove the whole audio stack. Instead we pin our bluetooth
#   package to the STOCK base version (= 1.4.2-1+rptN), so it installs alone
#   against the untouched stack. If the Pi later upgrades pipewire, the held
#   package's dependency breaks loudly instead of silently mixing ABIs —
#   rebuild against the new version at that point.
# - The build runs inside an emulated arm64 Debian Trixie container
#   (qemu binfmt), so it is SLOW (an hour-ish). Let it run in the foreground.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p out

# Host uid/gid so the .debs we cp into out/ are owned by the invoking user,
# not root (the container build runs as root because apt needs it).
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

# Bump this when the RPi archive rotates its keyring package
# (https://archive.raspberrypi.com/debian/pool/main/r/raspberrypi-archive-keyring/).
KEYRING_DEB_URL="https://archive.raspberrypi.com/debian/pool/main/r/raspberrypi-archive-keyring/raspberrypi-archive-keyring_2025.1%2Brpt1_all.deb"

# Ensure qemu arm64 emulation is registered (cross-build.sh uses a native
# cross-compiler and does NOT set this up). No-op if already installed.
if ! docker run --rm --platform linux/arm64 debian:trixie true 2>/dev/null; then
  echo "==> Registering qemu arm64 binfmt handler (one-time, needs --privileged)..."
  docker run --privileged --rm tonistiigi/binfmt --install arm64
fi

echo "==> Building patched libspa-0.2-bluetooth for arm64 (emulated; this is slow)..."
docker run --rm --platform linux/arm64 \
  -e HOST_UID="$HOST_UID" -e HOST_GID="$HOST_GID" \
  -e KEYRING_DEB_URL="$KEYRING_DEB_URL" \
  -v "$PWD":/w -w /build debian:trixie bash -euxc '
  export DEBIAN_FRONTEND=noninteractive
  export DEBEMAIL="prodigy@openauto" DEBFULLNAME="openauto-prodigy"
  # Enable deb-src for Debian, then add the Raspberry Pi OS archive (deb +
  # deb-src) — its pipewire (+rptN) is what the Pi actually runs.
  sed -i "s/^Types: deb$/Types: deb deb-src/" /etc/apt/sources.list.d/debian.sources
  apt-get update
  apt-get install -y --no-install-recommends ca-certificates curl
  curl -fsSL -o /tmp/rpi-keyring.deb "$KEYRING_DEB_URL"
  dpkg-deb --fsys-tarfile /tmp/rpi-keyring.deb \
    | tar -x -C / ./usr/share/keyrings/raspberrypi-archive-keyring.pgp
  cat > /etc/apt/sources.list.d/raspi.sources <<EOF
Types: deb deb-src
URIs: http://archive.raspberrypi.com/debian
Suites: trixie
Components: main
Signed-By: /usr/share/keyrings/raspberrypi-archive-keyring.pgp
EOF
  apt-get update
  apt-get install -y --no-install-recommends build-essential devscripts quilt
  apt-get build-dep -y pipewire
  # apt-get source picks the highest version — the RPi +rptN rebuild.
  apt-get source pipewire
  cd pipewire-*/
  BASE_VERSION=$(dpkg-parsechangelog -S Version)
  echo "$BASE_VERSION" | grep -q "+rpt" \
    || { echo "ERROR: expected the +rptN source from archive.raspberrypi.com"; exit 1; }
  # Debian source format 3.0 (quilt); patches live in debian/patches.
  export QUILT_PATCHES=debian/patches
  quilt import /w/0001-hfp-disable-lc3-swb.patch
  quilt push
  # Pin the modules dependency to the STOCK base version (see header note) so
  # our single deb installs against the untouched stack. The sed hits every
  # stanza with that dependency; only libspa-0.2-bluetooth is shipped, so the
  # others do not matter.
  sed -i "s/libspa-0.2-modules (= \${binary:Version})/libspa-0.2-modules (= $BASE_VERSION)/" debian/control
  grep -q "libspa-0.2-modules (= \${binary:Version})" debian/control \
    && { echo "ERROR: dependency pin sed did not take"; exit 1; }
  grep -q "libspa-0.2-modules (= $BASE_VERSION)" debian/control \
    || { echo "ERROR: pinned dependency not present after sed"; exit 1; }
  # +prodigyN local version bump keeps the base version intact, e.g.
  # 1.4.2-1+rpt3 -> 1.4.2-1+rpt3+prodigy1, which apt treats as an upgrade
  # over the installed +rpt3 (no --allow-downgrades needed).
  dch --local +prodigy "Disable HFP LC3-SWB advertisement (silent uplink); force mSBC."
  # parallel speeds the emulated build; nocheck skips pipewire own test suite
  # (we only need the bluez5 plugin; the suite is slow/flaky under qemu).
  export DEB_BUILD_OPTIONS="parallel=$(nproc) nocheck"
  dpkg-buildpackage -b -uc -us
  # Only the bluez5 plugin package ships (dependency pinned to the stock
  # stack, see header). The trailing underscore excludes -dbgsym packages.
  cp ../libspa-0.2-bluetooth_*.deb /w/out/
  chown "$HOST_UID:$HOST_GID" /w/out/*.deb
'
echo "Staged: $(ls out/)"
