#!/usr/bin/env bash
set -euo pipefail
PROJECT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM="${UPSTREAM:-$PROJECT/.upstream/espkvm}"
REV="1983a0adf2474e96026a7f46357f7812eb5e6960"
if [[ ! -d "$UPSTREAM/.git" ]]; then
  mkdir -p "$(dirname "$UPSTREAM")"
  git clone --recursive https://github.com/espkvm/espkvm.git "$UPSTREAM"
fi
cd "$UPSTREAM"
if [[ "$(git rev-parse HEAD)" != "$REV" ]]; then
  git fetch origin "$REV"
  git checkout --detach "$REV"
  git submodule update --init --recursive
fi
python3 "$PROJECT/scripts/apply.py" "$UPSTREAM"
command -v idf.py >/dev/null || { echo "ESP-IDF 6.1 fehlt. Aktiviere ~/esp/esp-idf/export.sh" >&2; exit 1; }
idf.py -B build.cam-nano-rev3 -D SDKCONFIG=build.cam-nano-rev3/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;boards/nano_p4.defaults;boards/nano_p4_rev3.defaults' build
echo "Firmware gebaut. Flash: cd .upstream/espkvm && idf.py -B build.cam-nano-rev3 -p /dev/ttyACM0 flash monitor"
