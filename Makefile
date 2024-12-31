# MAC OS ONLY!
#
# This Makefile is only there to help building the MacOS DMG files, codesign them, notarize and staple
# and verify all of this. If you are using a normal, sane OS, you can ignore this Makefile
# and do the regular CMake process. Checkout the .github/workflows files for the authoritative way to build
# for your platform
#

BUILD_DIR=.builds/universal_again
VERSION=2.4.4.73
TEAM_ID=98WQ3X9M7Q
KEYCHAIN_PROFILE=christof-2025

# Make sure to setup a Python that matches the universal build/fat binary or the architecture build
# This can be really messy if you have - like me - multiple versions of Python installed on the Mac.
# Closely watch the build output to detect and inconsistencies during configuration, linking, or bundle fixing!
# Worst case, uninstall all Pythons except the one you want to use.
PYTHON_TO_USE=/Library/Frameworks/Python.framework/Versions/Current/bin/python3

# Setup variables for the various build artifacts and their names
KNOBKRAFT=KnobKraft_Orm-$(VERSION)-Darwin
KNOBKRAFT_APP=$(BUILD_DIR)/The-Orm/KnobKraftOrm.app
KNOBKRAFT_DMG=$(BUILD_DIR)/$(KNOBKRAFT).dmg
KNOBKRAFT_MOUNT=/Volumes/$(KNOBKRAFT)
KNOBKRAFT_MOUNTED_APP=$(KNOBKRAFT_MOUNT)/KnobKraftOrm.app

all: configure build sign-dmg verify-signed

apple: notarize staple verify-notarization

configure:
	cmake -S . -B $(BUILD_DIR) -DPYTHON_EXECUTABLE=$(PYTHON_TO_USE)

build:
	cmake --build $(BUILD_DIR) --target package -- -j 6

$(KNOBKRAFT_MOUNT): $(KNOBKRAFT_DMG)
	hdiutil detach $@ ; true
	yes | PAGER=cat hdiutil attach $<

attach: $(KNOBKRAFT_MOUNT)

detach: $(KNOBKRAFT_MOUNT)/KnobKraftOrm.app
	hdiutil detach $(KNOBKRAFT_MOUNT)

app-signed: attach
	codesign --verify -v $(KNOBKRAFT_MOUNTED_APP)

binary-signed: $(KNOBKRAFT_APP)
	codesign --verify -v $(KNOBKRAFT_APP)

dmg-signed: $(KNOBKRAFT_DMG)
	codesign --verify -v $<

verify-signed: binary-signed app-signed dmg-signed

sign-dmg: $(KNOBKRAFT_DMG)
	codesign --force --verbose=2 --sign "$(TEAM_ID)" $<

.PHONY: notarize
notarize: $(KNOBKRAFT_DMG)
	xcrun notarytool submit $< \
		--keychain-profile "$(KEYCHAIN_PROFILE)" \
		--wait

staple: $(KNOBKRAFT_DMG)
	xcrun stapler staple $<

verify-notarization: $(KNOBKRAFT_DMG)
	xcrun spctl --assess --type open --context context:primary-signature --ignore-cache --verbose=2 $<
	xcrun spctl --assess --type install --ignore-cache --verbose=2 $<

run-app: $(KNOBKRAFT_APP)
	open $<

run-dmg: attach
	open $(KNOBKRAFT_MOUNTED_APP)

kill:
	killall KnobKraftOrm

realclean:
	rm -rf $(BUILD_DIR)
