==========================================================================================
Prophet-6 Factory Program Banks ReadMe File                                       
July-23-2015
==========================================================================================

The accompanying file, P6_Programs_v1.01.syx, contains all of the factory program banks
for the Prophet-6. The data is in a MIDI System Exclusive (SysEx) file. You will need a
DAW, MIDI utility, or other application capable of opening and sending MIDI System
Exclusive (SysEx) messages. MIDI-OX (Windows) and SysEx Librarian (Mac OS) are shareware
MIDI utilities that can reliably be used to update DSI instruments. You will also need
either a USB cable (Type A to Type B connectors, like a typical USB printer cable) or a
MIDI interface to transmit the SysEx file from your computer to the Prophet-6.

You will find instructions for updating using MIDI-OX and SysEx Librarian below.

************************************* IMPORTANT NOTE *************************************

Be aware that loading the factory programs overwrites the user programs that are currently
in memory. If you have programs you want to keep, be sure to save them to a SysEx file
first. Refer to the manual for more information about initiating a SysEx dump from the
Prophet-6’s front panel controls.

******************************************************************************************

__________________________________________________________________________________________
LOADING PROGRAMS FROM WINDOWS USING MIDI-OX

Before starting, close all other audio or MIDI or DAW software, and disconnect all other
MIDI devices. If necessary, download and install MIDI-OX.

http://www.midiox.com/

To prepare the Prophet-6 to receive system exclusive messages:

1. Press the Globals button once.

2. Press program selector #8 (MIDI Sysex).

3. From the Global menu, select "MIDI Sysex Cable."

4. Use the Bank and Tens buttons to choose either USB or MIDI depending on your preferred
method and connections. If you're using USB from your computer, choose "USB." If you're
using a MIDI interface, choose "MIDI" (the display shows MIDI as "nid").

5. Press the Globals button twice to exit the Global menu.

Connect your computer to the Prophet-6 using either a USB cable or a MIDI interface.

If using USB, the Prophet-6 is a Class Compliant USB device. That means it does not
require any additional drivers to be installed to communicate with your computer. It will
appear in MIDI-OX and other MIDI applications as a MIDI port named "Prophet-6." (Under
Windows XP, it will appear as "USB Audio Device.")

If using a MIDI interface, connect the computer's MIDI out to the Prophet-6's MIDI in.
Depending on the type of interface, you may also need a MIDI cable.


To load the programs:

 1. Run MIDI-OX.

 2. From the Options menu, choose "MIDI Devices."

 3. In the lower left quadrant of the window, select the MIDI port to which the instrument
 is connected. If connected via USB, choose "Prophet-6" (or "USB Audio Device" under
 Windows XP). If connected via MIDI, choose the MIDI interface. The selected MIDI port
 appears in the Port Mappings window.

 4. Click OK to close the dialog and save the settings.

 5. From the View menu, choose "SysEx."

 6. From the SysEx menu, choose "Configure."

 7. Set the Low Level Output Buffers "Num" and "Size" to 1024.

 8. Under Output Timing, make sure that "Auto-adjust Buffer Delays if necessary" is NOT
 enabled.

 9. Click OK to close the dialog and save the settings.

10. From the Command Window menu, choose "Load File." Browse to and open the Prophet-6
programs file.

11. From the Command Window menu, choose "Send SysEx."The programs will increment on the
Prophet-6 as each program file is transferred to it.

__________________________________________________________________________________________
LOADING PROGRAMS FROM A MAC USING SYSEX LIBRARIAN

Before starting, close all other audio or MIDI or DAW software, and disconnect all other
MIDI devices. If necessary, download and install SysEx Librarian.

www.snoize.com/sysexlibrarian

To prepare the Prophet-6 to receive system exclusive messages:

1. Press the Globals button once.

2. Press program selector #8 (MIDI Sysex).

3. From the Global menu, select "MIDI Sysex Cable."

4. Use the Bank and Tens buttons to choose either USB or MIDI depending on your preferred
method and connections. If you're using USB from your computer, choose "USB." If you're
using a MIDI interface, choose "MIDI" (the display shows MIDI as "nid").

5. Press the Globals button twice to exit the Global menu.

Connect your computer to the Prophet-6 using either a USB cable or a MIDI interface.

If using USB, the Prophet-6 is a Class Compliant USB device. That means it does not
require any additional drivers to be installed to communicate with your computer. It will
appear in SysEx Librarian, the Mac's Audio MIDI Utility, and other MIDI applications as a
MIDI port named "Prophet-6."

If using a MIDI interface, connect the computer's MIDI out to the Prophet-6's MIDI in.
Depending on the type of interface, you may also need a MIDI cable.

To load and update the OS:

1. Run SysEx Librarian.

2. From the SysEx Librarian menu, choose "Preferences."

3. Close the SysEx Librarian Preferences window to save changes.

4. Choose your MIDI interface from the "Destination" menu.

5. Click Add/+ to add the file to the file list or simply drag and drop the file on the
open SysEx Librarian window. The file name appears in the file list and should be
highlighted.

6. Click Play.The programs will increment on the Prophet-6 as each program file is
transferred to it.
