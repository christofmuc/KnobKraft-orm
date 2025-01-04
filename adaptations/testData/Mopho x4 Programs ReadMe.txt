==========================================================================================
Mopho x4 Factory Program Banks ReadMe File                                     Aug-29-2014
==========================================================================================

The accompanying file, Mopho_x4_AllBanks_V1.01.syx, contains all of the factory program 
banks for the Mopho x4. The data is in a MIDI System Exclusive (SysEx) file that can be 
loaded into the Mopho x4 via MIDI using a MIDI utility or other application capable of 
opening and transmitting SysEx files. The simplest method may be to use a shareware MIDI 
utility such as SysEx Librarian (Mac OS) or MIDI-OX (Windows). These can be downloaded 
from:

http://www.snoize.com/SysExLibrarian/

http://www.midiox.com/

You will also need a USB cable or a MIDI interface to connect your computer to the Mopho 
x4.

There is nothing you need to do to prepare the Mopho x4 to receive the programs. Once the 
transfer begins, the programs will automatically be written to the Mopho x4's memory.

************************************* IMPORTANT NOTE *************************************

Be aware that loading programs overwrites any programs that are currently in memory. If 
you have programs you want to keep, be sure to save them to a SysEx file first. Refer to 
the manual for more information about initiating a SysEx dump from the Mopho x4's front 
panel controls.

******************************************************************************************

__________________________________________________________________________________________
LOADING PROGRAMS FROM A MAC USING SYSEX LIBRARIAN

Before starting, close all other audio or MIDI or DAW software, and disconnect all other 
MIDI devices. If necessary, download and install SysEx Librarian.

http://www.snoize.com/SysExLibrarian/

Connect the Mopho x4 to your computer using either a MIDI interface or USB.

Via MIDI:
Connect the MIDI out of your computer's MIDI interface to the Mopho x4's MIDI in. 
Depending on the type of MIDI interface, you may also need a MIDI cable to connect the 
interface to Mopho x4.

Via USB:
Connect the Mopho x4 directly to your computer using a USB cable (Type A to Type B 
connectors, like a typical USB printer cable). The Mopho x4 is a Class Compliant USB 
device. That means it does not require any additional drivers to be installed to 
communicate with your computer. It will appear in SysEx Librarian, the Mac's Audio MIDI 
Utility, and other MIDI applications as a MIDI port named "DSI Mopho x4."

To load the programs:
1. Run SysEx Librarian.
2. If connected via MIDI, choose your MIDI interface from the "Destination" menu. If 
   connected via USB, choose "DSI Mopho x4."
3. Click Add/+ to add the file to the file list or simply drag and drop the file on the 
   open SysEx Librarian window.
	The file name appears in the file list and should be highlighted.
4. Click Play.
	You'll see the programs changing on the Mopho x4 as the they are being written.
	
When the transmission completes, you're done!

If the load fails to start, make sure the Mopho x4 is actually receiving MIDI data. On the 
MIDI Channel page of the Global menu, a dot blinks between "MIDI" and "Channel" when  MIDI 
data is received. Also, make sure that MIDI SysEx in the Global menu is set to "On."

__________________________________________________________________________________________
LOADING PROGRAMS FROM WINDOWS USING MIDI-OX

Before starting, close all other audio or MIDI or DAW software, and disconnect all other 
MIDI devices. If necessary, download and install MIDI-OX.

http://www.midiox.com/

Connect the Mopho x4 to your computer using either a MIDI interface or USB.

Via MIDI:
Connect the MIDI out of your computer's MIDI interface to the Mopho x4's MIDI in. 
Depending on the type of MIDI interface, you may also need a MIDI cable to connect the 
interface to Mopho x4.

Via USB:
Connect the Mopho x4 directly to your computer using a USB cable (Type A to Type B 
connectors, like a typical USB printer cable). The Mopho x4 is a Class Compliant USB 
device. That means it does not require any additional drivers to be installed to 
communicate with your computer. It will appear in MIDI-OX and other MIDI applications as a 
MIDI port named "DSI Mopho x4." (Under Windows XP, it will appear as "USB Audio Device.")

To load the programs:
1. Run MIDI-OX.
2. From the Options menu, choose "Configure Buffers" and set the Low Level Output Buffers 
   Size and Num to 1024.
3. Click OK.
4. From the Options menu, choose "MIDI Devices."
5. In the lower left quadrant of the window, select the MIDI port to which the Mopho x4 is 
   connected.
	The selected MIDI port appears in the Port Mappings window.
6. Click OK.
7. From the View menu, choose SysEx.
	The SysEx window opens.
8. From the Command Window menu, choose "Load File."
	Browse to and open the SysEx file containing the programs.
9. From the Command Window menu, choose "Send SysEx."
	You'll see the programs changing on the Mopho x4 as the they are being written.

When the transmission completes, you're done!

If the load fails to start, make sure the Mopho x4 is actually receiving MIDI data. On the 
MIDI Channel page of the Global menu, a dot blinks between "MIDI" and "Channel" when MIDI 
data is received. Also, make sure that MIDI SysEx in the Global menu is set to "On."
