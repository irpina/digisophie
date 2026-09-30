#!/bin/sh
# Build the hardware-tested S027 configuration without redistributing firmware.
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 STOCK_DIGITAKT_OS_1.53.syx ELEKLOADER_CHECKOUT" >&2
    exit 2
fi

stock=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
loader=$(cd "$2" && pwd)
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python=${PYTHON_BIN:-python3}

if [ ! -f "$stock" ] || [ ! -f "$loader/elekloader/sdk/build.py" ]; then
    echo "Expected a stock OS file and an elekloader source checkout." >&2
    exit 2
fi

cd "$loader"
"$python" -m elekloader.sdk.build mods/core --stock "$stock"
"$python" -m elekloader.sdk.build "$project" --stock "$stock"
(
    cd "$project/diagnostics/digihealth"
    PYTHONPATH="$loader${PYTHONPATH:+:$PYTHONPATH}" "$python" build.py --stock "$stock"
)

"$python" -m elekloader.lint --stock "$stock" \
    "$loader/mods/core/out/core-2.1.elemod" \
    "$project/diagnostics/digihealth/out/digihealth-1.0.1.elemod" \
    "$project/out/digisophie-0.1.7.elemod"
"$python" -m elekloader.patch --stock "$stock" \
    --mod "$loader/mods/core/out/core-2.1.elemod" \
    --mod "$project/diagnostics/digihealth/out/digihealth-1.0.1.elemod" \
    --mod "$project/out/digisophie-0.1.7.elemod" \
    --out "$project/out/Digitakt_OS1.53_SOPHIE_S027.syx" --version S027
