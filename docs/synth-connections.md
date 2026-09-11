# Checking and finding synths

After turning on a synth connected through a MIDI interface, click the status strip below its name to **Check connection**. KnobKraft checks the saved MIDI output and channel. Clicking the name still selects the synth for browsing, including when it is offline. Green records a successful check; it is not a continuous connection monitor.

If no connection has been saved yet, clicking the strip searches the MIDI outputs for that synth. If a saved connection does not respond, the dialog offers **Try again** or **Find on other ports...**. A failed check does not automatically start a full search or erase the saved connection.

Use **Find this synth...** after connecting a synth for the first time or moving its cables. This searches all MIDI outputs for only that synth. It is available by right-clicking the synth button or status strip, and in the synth's Setup section. The Setup section also has a **Check connection** button.

For multiple synths, Setup and the MIDI menu offer:

- **Check saved connections** (F2): check each enabled synth's saved output and channel. Use this after turning synths on.
- **Find all synths...** (F1): search all MIDI outputs for every enabled synth. Use this for initial setup or after rewiring several synths.

USB port changes continue to trigger an automatic check of saved connections. Synth adaptations that do not support detection must be configured manually in Setup. MIDI port exclusion is not part of this change.

## Verification with hardware

1. Connect and detect two synths on different MIDI inputs. Turn one DIN-connected synth off before launching KnobKraft, then turn it on and click its grey status strip. It should turn green without searching other outputs or changing the selected synth.
2. Leave the synth off. Check its connection and verify that the dialog names the saved output and channel. Cancel without launching a search, then retry after turning it on.
3. Move that synth to another output and choose **Find this synth...**. Confirm that the new route is saved and other synths still receive MIDI. Repeat while cancelling the search.
4. With no saved route, click the grey strip and verify that only that synth is searched for. Check that the strip is also shown when only one synth is enabled.
5. Use F2 and F1 to check saved connections and find all synths respectively. Confirm that the MIDI menu labels match.
6. Attach a USB synth while a search is running. The automatic check should run after the manual search ends.

Hardware-independent regression tests can be enabled with `MIDIKRAFT_BUILD_DETECTION_TESTS=ON` and `JUCE_WIDGETS_BUILD_CONNECTION_TESTS=ON`. Build and run the `midikraft_detection_tests` and `synth_connection_tests` targets. These cover saved-route validation, cancellation before probing, unsupported adaptations, input restoration, and independent selection/status actions.
