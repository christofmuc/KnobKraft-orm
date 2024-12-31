BUILD_DIR=.builds/universal_again
VERSION=2.4.4.70

KNOBKRAFT=KnobKraft_Orm-$(VERSION)-Darwin
KNOBKRAFT_APP=$(BUILD_DIR)/The-Orm/KnobKraftOrm.app
KNOBKRAFT_DMG=$(BUILD_DIR)/$(KNOBKRAFT).dmg
KNOBKRAFT_MOUNT=/Volumes/$(KNOBKRAFT)

$(KNOBKRAFT_MOUNT): $(KNOBKRAFT_DMG)
	hdiutil detach $@ ; true
	yes | PAGER=cat hdiutil attach $<

attach: $(KNOBKRAFT_MOUNT)

detach: $(KNOBKRAFT_MOUNT)/KnobKraftOrm.app
	hdiutil detach $(KNOBKRAFT_MOUNT)

app-signed: attach
	codesign -v -v $(KNOBKRAFT_MOUNT)/KnobKraftOrm.app

binary-signed: $(KNOBKRAFT_APP)
	codesign -v -v $(KNOBKRAFT_APP)

dmg-signed: $(KNOBKRAFT_DMG)
	codesign -v -v $<

signed: binary-signed app-signed dmg-signed

