#!/bin/zsh

# python3 /opt/homebrew/lib/python3.9/site-packages/pyinstaller/utils/cliutils/archive_viewer.py -h
export CMAKE_BINARY_DIR=./builds-new

# rm -rf build
# rm -rf dist
pyinstaller --clean -y KnobKraft.spec --workpath "$CMAKE_BINARY_DIR/pywork" --distpath "$CMAKE_BINARY_DIR"/pydist --log-level=DEBUG

rm -rf KnobKraftShim.pkg_extracted
(mkdir "$CMAKE_BINARY_DIR"/pyextras && cd "$CMAKE_BINARY_DIR"/pyextras && python3 ../_deps/pyinstxtractor-src/pyinstxtractor.py ../pywork/KnobKraft/KnobKraftShim.pkg)

(cd "$CMAKE_BINARY_DIR"/pyextras/KnobKraftShim.pkg_extracted/PYZ-00.pyz_extracted/ && zip -r ../../../The-Orm/KnobKraftOrm.app/Contents/MacOS/python_modules.zip *)
(cd "$CMAKE_BINARY_DIR"/pywork/KnobKraft/localpycs/ && zip -r ../../../The-Orm/KnobKraftOrm.app/Contents/MacOS/python_modules.zip *)
# Copy all additional files added by pyinstaller to the dist directory into our target App directory
cp -R "$CMAKE_BINARY_DIR"/pydist/KnobKraft/* "$CMAKE_BINARY_DIR"/The-Orm/KnobKraftOrm.app/Contents/MacOS/
# Remove the shim, as we really don't want it to be part of our App bundle in the end
rm "$CMAKE_BINARY_DIR"/The-Orm/KnobKraftOrm.app/Contents/MacOS/KnobKraftShim
