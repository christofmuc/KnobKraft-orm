==========================================================================================
Prophet 12 Factory Program Banks ReadMe File                                   Aug-27-2014
==========================================================================================

The accompanying file, P12_Programs_v1.1c.syx, contains all of the factory program banks 
for the Prophet 12. The data is in a MIDI System Exclusive (SysEx) file. You will need a 
DAW, MIDI utility, or other application capable of opening and sending MIDI System 
Exclusive (SysEx) messages. MIDI-OX (Windows) and SysEx Librarian (Mac OS) are shareware 
MIDI utilities that can reliably be used to update DSI instruments. You will also need 
either a USB cable (Type A to Type B connectors, like a typical USB printer cable) or a 
MIDI interface to transmit the SysEx file from your computer to the Prophet 12.

You will find instructions for updating using MIDI-OX and SysEx Librarian below.

************************************* IMPORTANT NOTE *************************************

Be aware that loading programs overwrites any programs that are currently in memory. If 
you have programs you want to keep, be sure to save them to a SysEx file first. Refer to 
the manual for more information about initiating a SysEx dump from the Prophet 12's front 
panel controls.

******************************************************************************************

__________________________________________________________________________________________
LOADING PROGRAMS FROM WINDOWS USING MIDI-OX

Before starting, close all other audio or MIDI or DAW software, and disconnect all other 
MIDI devices. If necessary, download and install MIDI-OX.

http://www.midiox.com/

To prepare the Prophet 12 to receive system exclusive messages:

1. Press Global.

2. From the Global menu, select "MIDI Sysex Enable" and make sure it's set to "On."

3. From the Global menu, select "MIDI Sysex Cable."

4. If you're using USB, choose "USB." If you're using a MIDI interface, choose "MIDI."

5. Press Global again to exit the Global menu.

Connect your computer to the Prophet 12 using either a USB cable or a MIDI interface.

If using USB, the Prophet 12 is a Class Compliant USB device. That means it does not 
require any additional drivers to be installed to communicate with your computer. It will 
appear in MIDI-OX and other MIDI applications as a MIDI port named "Prophet 12 Keyboard" 
or "Prophet 12 Module." (Under Windows XP, it will appear as "USB Audio Device.")

If using a MIDI interface, connect the computer's MIDI out to the Prophet 12's MIDI in. 
Depending on the type of interface, you may also need a MIDI cable.

Most MIDI interfaces or sound cards with built-in MIDI interfaces will work. However, we 
have received reports that Digidesign/Avid interfaces and Native Instruments Maschine may 
not transmit SysEx correctly for updates. The M-Audio Uno is an inexpensive, reliable, and 
widely available MIDI interface for use when updating our instruments.

To load the programs:

 1. Run MIDI-OX.

 2. From the Options menu, choose "MIDI Devices."

 3. In the lower left quadrant of the window, select the MIDI port to which the instrument 
    is connected. If connected via USB, choose "Prophet 12 Keyboard" or "Prophet 12 
    Module" (or "USB Audio Device" under Windows XP). If connected via MIDI, choose the 
    MIDI interface.
	The selected MIDI port appears in the Port Mappings window.

 4. Click OK to close the dialog and save the settings.

 5. From the View menu, choose "SysEx."

 6. From the SysEx menu, choose "Configure."

 7. Set the Low Level Output Buffers "Num" and "Size" to 1024.
 
 8. Under Output Timing, make sure that "Auto-adjust Buffer Delays if necessary" is NOT 
    enabled.

 9. Click OK to close the dialog and save the settings.

10. From the Command Window menu, choose "Load File."
	Browse to and open the Prophet 12 programs file.
	
11. From the Command Window menu, choose "Send SysEx."
	The programs will increment on the Prophet 12 as each program file is transferred to 
	it.

__________________________________________________________________________________________
LOADING PROGRAMS FROM A MAC USING SYSEX LIBRARIAN

Before starting, close all other audio or MIDI or DAW software, and disconnect all other 
MIDI devices. If necessary, download and install SysEx Librarian.

www.snoize.com/sysexlibrarian

To prepare the Prophet 12 to receive system exclusive messages:

1. Press Global.

2. From the Global menu, select "MIDI Sysex Enable" and make sure it's set to "On."

3. From the Global menu, select "MIDI Sysex Cable."

4. If you're using USB, choose "USB." If you're using a MIDI interface, choose "MIDI."

5. Press Global again to exit the Global menu.

Connect your computer to the Prophet 12 using either a USB cable or a MIDI interface.

If using USB, the Prophet 12 is a Class Compliant USB device. That means it does not 
require any additional drivers to be installed to communicate with your computer. It will 
appear in SysEx Librarian, the Mac's Audio MIDI Utility, and other MIDI applications as a 
MIDI port named "Prophet 12 Keyboard" or "Prophet 12 Module."

If using a MIDI interface, connect the computer's MIDI out to the Prophet 12's MIDI in. 
Depending on the type of interface, you may also need a MIDI cable.

Most MIDI interfaces or sound cards with built-in MIDI interfaces will work. However, we 
have received reports that Digidesign/Avid interfaces and Native Instruments Maschine may 
not transmit SysEx correctly for updates. The M-Audio Uno is an inexpensive, reliable, and 
widely available MIDI interface for use when updating our instruments.

To load and update the OS:

1. Run SysEx Librarian.

2. From the SysEx Librarian menu, choose "Preferences."

3. Close the SysEx Librarian Preferences window to save changes.

4. Choose your MIDI interface from the "Destination" menu.

5. Click Add/+ to add the file to the file list or simply drag and drop the file on the 
   open SysEx Librarian window.
	The file name appears in the file list and should be highlighted.
	
6. Click Play.
	The programs will increment on the Prophet 12 as each program file is transferred to 
	it.
