# OpenAuto Prodigy

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Open-source replacement for OpenAuto Pro — a Raspberry Pi-based Android Auto head unit application.

**Status: Work in Progress**

## Target Platform

- Raspberry Pi 4/5
- RPi OS Trixie (Debian 13, 64-bit)
- Qt 6.8

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## License

This project is licensed under the GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
