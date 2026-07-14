#!/bin/sh
# unpack repo assets that are too large for github as-is.
# run once after cloning: sh scripts/setup_assets.sh
cd "$(dirname "$0")/.."
if [ ! -f assets/models/garage/garage.obj ]; then
    unzip -o assets/models/garage/garage_obj.zip -d assets/models/garage
    echo "unpacked assets/models/garage/garage.obj"
else
    echo "garage.obj already unpacked"
fi
