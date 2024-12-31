BUILD_DIR=.builds/universal_again
VERSION=2.4.4.70
TEAM_ID=98WQ3X9M7Q

KNOBKRAFT=KnobKraft_Orm-$(VERSION)-Darwin
KNOBKRAFT_APP=$(BUILD_DIR)/The-Orm/KnobKraftOrm.app
KNOBKRAFT_DMG=$(BUILD_DIR)/$(KNOBKRAFT).dmg
KNOBKRAFT_MOUNT=/Volumes/$(KNOBKRAFT)
KNOBKRAFT_MOUNTED_APP=$(KNOBKRAFT_MOUNT)/KnobKraftOrm.app

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

signed: binary-signed app-signed dmg-signed

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

run-app: $(KNOBKRAFT_APP)
	open $<

run-dmg: attach
	open $(KNOBKRAFT_MOUNTED_APP)

kill:
	killall KnobKraftOrm

realclean:
	rm -rf $(BUILD_DIR)
