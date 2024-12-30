BUILD_DIR=builds/universal_xcode
CMAKE=cmake
CODESIGN=codesign
CPACK=cpack

BUILD_TYPE=Debug
DMG_FILE=KnobKraft_Orm-2.4.4.64-Darwin.dmg
TEAM_ID="98WQ3X9M7Q"

all: configure build package sign-dmg verify-app verify-dmg notarize staple verify-notarization

codesign-only: configure build package sign-dmg verify-app verify-dmg

.phony: configure
configure:
	$(CMAKE) -G Xcode -S . -B $(BUILD_DIR) -DPYTHON_EXECUTABLE=/Library/Frameworks/Python.framework/Versions/Current/bin/python3 -DTEAM_ID=$(TEAM_ID) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.phony: build
build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_TYPE)

package: build
	$(CPACK) -G DragNDrop -B $(BUILD_DIR) --config $(BUILD_DIR)/CPackConfig.cmake -C $(BUILD_TYPE)

sign-dmg: build
	$(CODESIGN) --force --verbose=2 --sign "$(TEAM_ID)" $(BUILD_DIR)/$(DMG_FILE)

verify-app: build
	$(CODESIGN) --verify --verbose=2 $(BUILD_DIR)/MyMacOSApp.app

verify-dmg: sign-dmg
	$(CODESIGN) --verify --verbose=2 $(BUILD_DIR)/$(DMG_FILE)

notarize: dist/MyMacOSApp-0.1.1-Darwin.dmg
	xcrun notarytool submit ./dist/MyMacOSApp-0.1.1-Darwin.dmg \
		--keychain-profile "$(KEYCHAIN_PROFILE)" \
		--wait

staple: dist/MyMacOSApp-0.1.1-Darwin.dmg
	xcrun stapler staple ./dist/MyMacOSApp-0.1.1-Darwin.dmg

verify-notarization: dist/MyMacOSApp-0.1.1-Darwin.dmg
	xcrun spctl --assess --type open --context context:primary-signature --ignore-cache --verbose=2 ./dist/MyMacOSApp-0.1.1-Darwin.dmg

run: build
	open $(BUILD_DIR)/The-Orm/$(BUILD_TYPE)/KnobKraftOrm.app

kill: build
	killall KnobKraftOrm

clean:
	rm -rf dist
