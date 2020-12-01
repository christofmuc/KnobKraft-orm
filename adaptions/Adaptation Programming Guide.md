# KnobKraft Orm Adaptation Programming Guide

The KnobKraft Orm Sysex Librarian uses "adaptation files" which contain a bit of Python code to generate and process the MIDI messages it needs to talk to a specific synthesizer device. Think of it as a very specific device driver that allows the librarian to work with a specific sysex dialect of a device.

It is fairly simple and quick to create a new Adaptation if you have done it before, but it can be quite an adventure if you are slightly out of your comfort zone. I will try to explain the main steps to take here and further down you will find the reference documentation, but if you consider doing an Adaptation for your device, please check upfront the following prerequisites:

* You have an understanding of a byte and a bit, and can understand hexadecimal notation. Most of the notation you will find changes between decimal, binary, and hexadecimal.
* There is a programming manual or MIDI implementation document for the device you want to implement. Sometimes this is right in the User Manual (thank you DSI for putting it into the Rev2's manual!), sometimes it is in the Service Manual you'll be able to find in the Internet (e.g. the Korg DW8000 Service Manual had all the information I needed), sometimes the manufacturer is willing to provide it if you ask nicely. Sadly, many manufacturers today refuse to hand out Sysex documentation although they use Sysex (I asked Waldorf for the Kyra documents, and they just said "no"), or they themselves encourage you to reverse engineer (Arturia basically said "just look at what our software sends to the Keystep, it's easy to figure out"). Reverse engineering also is possible sometimes, but out of scope for this document.
* The MIDI implementation of the device is suited for the current capabilities of the Adaptation interface. This is the case when e.g. it reacts on program change messages and a request edit buffer and send edit buffer function is given. The edit buffer/program should also come as a single message, devices that reply with a stream of messages are currently not supported. Also, complex devices with many different data types are currently not really well supported by the Adaptations, only by the main program. If in doubt, please drop me a message before you spend energy on a futile endeavour!
* You are willing to edit a bit of Python code. No need to learn the complete language, even if I encourage everyone to strive to do it, as it is really neat, but we'll use only very basic functions. What is more important is that you use an editor that knows Python and is able to point out formatting and the horrible tabs vs spaces mistakes. I use PyCharm Community edition, but others will work as well.

## Architecture

The KnobKraft Orm is a C++ program, and the whole program logic, MIDI communication, and even some synthesizer implementations are contained in various C++ submodules. So normally, to add a device to the Orm you would need to create a new C++ module. As an example, here is a link to my implementation of the Yamaha RefaceDX: https://github.com/christofmuc/MidiKraft-yamaha-refacedx

Implementing each device as its own C++ module quickly became too much effort, and honestly also repetitive, so to the power of abstraction I decided to create one "last" device in C++: The GenericAdaptation class. This class implements all functions the C++ program expects, but delegates the execution of the function into a loaded Python script which implements a fixed list of functions (some of them are optional, some must be present for the module to work).

Technically, the C++ program uses Python in so called embedded mode, and the Python interpreter is executed in the same process as the main program, which is why you sadly can also crash or hang the main program by making mistakes in the Python code. Don't worry, it happens to everybody.

## Creating a new Adaptation

New adaptations are stored as a single Python file with the ending `.py` in a directory on your computer, and are read in on start of the KnobKraft Orm. To find out or change the directory where those are stored, press the button `Set User Adaptation Dir` in the Setup tab.

[![](SetupTab.PNG)]

If you accept the directory you chose, press the button next to it `Create new adaptation`. A dialog will pop up and show you a list of adaptations available to copy out.

**Important: The file copied is an exact copy of the code for the device you selected. The presence of this file will override the built-in version of the adaptation for this synth. The first thing you want to do to the newly created file is probably to change the value returned by the name() function, because this defines the identity of the device.**

Whenever you change the file, you will have to restart the KnobKraft Orm to load the new version. Pay close attention to the log messages of the Orm, they will tell you what went wrong in the Python code should an error occur. 

You can also use the `print()` statement in Python to print text into the log window of the Orm, which is very useful for debugging. Even more important might be the MIDI log showing you the communication betweem the Orm and the device.

## Where to start

The first thing, as I said before, is to change the text returned by the name() function - this name is the unique identifier for the device, so after you selected it please don't change it anymore if you want the database to find your patches again. 

Then you probably want to create a new test database via the File New... function. When I create new adaptations, I make sure not to pollute my production database with all my patches and favorites. You can switch between databases most easily with the File Recent... function.

After we chose the name and restartet the Orm, in the Setup tab we will see the name of the new device. Check mark it to enable, and probably turn off all other devices while you are working on the adaptation to make sure you see only the MIDI traffic from your code.

The minimum to implement are functions for the following tasks:

1. The device detection. The goal is to send a message which forces the device to reply, ideally revealing the MIDI channel or sysex ID the device is currently set to.
2. Requesting the edit buffer, and checking an arbitrary MIDI message if it is an edit buffer from our device
3. Requesting a specific program, and checking an arbitrary MIDI message if it is a program dump from our device
4. Turning any valid program message or edit buffer message into an edit buffer send command that will be sent to the device.

*Some synths do not have an edit buffer (the Kawai K3 for example), it is good practice to implement the edit buffer functions to use e.g. the highest program slot in order not to accidentally destroy the contant of program place 1. Remember to make a backup if possible at all of the device before you start sending any commmands to it.* 

## Data types

The data types used in the interface between the main program and the adaptation are deliberately very simple. These are:

1. Python strings for device and patch names
2. Python lists of integers that should not be bigger than 255 for raw MIDI messages
3. Python integer values for simple numbers like MIDI channels, program numbers, or milliseconds
4. Python booleans True or False for simple options and yes/no decisions

## List of functions to implement

For the device to function completely within the main program, you need to implement the following list functions not marked optional. The optional functions can be implemented for additional functionality.

I list all functions in the order they appear in the example file of the Korg DW6000 adaptation, grouped by the main function they are used for.

### Identity

    def name():

This function should return a string with the name of the device, uniquely identifying it. Don't make it too long, it is used on the buttons as well!

Example implementation:

    def name():
        return "Korg DW6000"

### Device detection

The device detection mechanism needs four functions to be implemented to work. And note that while many devices implement a version of a device ID request message or similar, sometimes it is smarter to do something completely different to coax the MIDI channel out of the device. Or to get it to react at all, you sometimes need to use brute force. E.g. to detect a Roland MKS-80 I initiate a full bank dump, and when the first data block comes just abort the transfer. Only then I know there is an MKS-80, and the MIDI channel is encoded in its response.

    def needsChannelSpecificDetection():

This function should return `True` if the `createDeviceDetectMessage()` should be called once for each of the 16 possible MIDI channels and MIDI outputs, or `False` if it should only be called once per MIDI output.

    def deviceDetectWaitMilliseconds():

You guessed it, return the number of milliseconds the main program will wait for the synth to answer before it moves on testing the next MIDI output. If this number is too low, you will get very confused because the synth will be detected on the wrong interface. Better start with a higher number, and when everything works bring this value down by experimenting. 200 ms is a good value for most device, I found.

Note that the main program will only wait once for all 16 channels, if you have channel specific detection turned on. The assumption is that there is no collision between the 16 different detect messages sent, and the device will only reply to one of them. 

    def createDeviceDetectMessage(channel):

This method should return a single MIDI message or multiple MIDI messages in the form of a single list of byte-values integers used to detect the device. 

Most often, this is the Identity Request message from the list of Universal Sysex messages (https://www.midi.org/specifications-old/item/table-4-universal-system-exclusive-messages), but it could be anything, really.

Example implementation for the Korg DW6000:

    def createDeviceDetectMessage(channel):
        # Page 5 of the service manual - Device ID Request
        # Different from the DW-8000, the DW-6000 does not differentiate which channel it is on via sysex
        return [0xf0, 0x42, 0x40, 0xf7]

Note that you need to specify the sysex start 0xf0 and end 0xf7. This is so you can also create non-sysex messages from within Python, e.g. CC or program change messages.

As the comment specifies, this message does not use the parameter `channel`, so it should return `False` in its implementation of the `needsChannelSpecificDetection` method. In case that returned true, this function will be called 16 times per MIDI out with the values 0..15 for `channel`.

Now to the core of the detection mechanism, the next method

    def channelIfValidDeviceResponse(message):

This function must return an integer: Either `-1`, if the message handed in (again a list of integers) is not the device response we had been expecting, or a valid MIDI channel `0..15` indicating which channel the device is currently configured for.

Here is the example implementation for the Korg DW6000:

    def channelIfValidDeviceResponse(message):
        # Page 3 of the service manual - Device ID
        if (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x42  # Korg
            and message[2] == 0x30  # Device ID
            and message[3] == 0x04):  # DW-6000
            # Sadly, there is no way to figure out which channel the Korg DW-6000 is set to
            return 1
        return -1

Basically I just check the first 4 bytes of the message. The `len()` check only prevents an index out of bounds exception should the message be shorter. When the first 4 bytes of the message match my expectations, I return 1 as a valid MIDI channel. In the case of the DW6000 there is no way to detect the channel, but that doesn't prevent the Librarian from working. The DW6000 as a low-budget synth simply was not equipped for the situation when somebody had two DW6000s!

With these four functions implemented, go ahead and test if the device detection works! Look at the MIDI log to watch the MIDI traffic. And remember that all synths need some kind of special setting/mode/switches to reply to sysex commands, better re-read that manual.

### Edit Buffer retrieval

To retrieve a patch from the edit buffer, two functions have to be implemented:

    def createEditBufferRequest(channel):

Similar to the `createDeviceDetectMessage`, this function should return a single MIDI message that makes the device send its Edit Buffer. In case the device doesn't support this operation, you can send it a command to return a specific program number maybe.

Example implementation for the DW6000:

    def createEditBufferRequest(channel):
        # Page 5 - Data Save Request
        return [0xf0, 0x42, 0x30, 0x04, 0x10, 0xf7]

You can see that I always include comments with references to the manual where I found the command, and I add the terminology used in the manual as well, because this is really different from vendor to vendor.

Second function to implement is very similar again, to check if a generic MIDI message is the reply to the request we just created:

    def isEditBufferDump(message):

Should return True or False appropriately. 

Here is the implementation for the DW6000:

    def isEditBufferDump(message):
        # Page 3 - Data Dump
        return (len(message) > 4
                and message[0] == 0xf0
                and message[1] == 0x42  # Korg
                and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
                and message[3] == 0x04  # DW-6000
                and message[4] == 0x40  # Data Dump
                )    
Important here is that in the third byte index by `message[2]` I want to only check the upper 4 bits and ignore the lower 4 bits, as they contain the MIDI channel and might be any value from 0..15. So with a binary and operator I mask the upper 4 bit, and compare only those.

*And yes, if you followed so far you see now that I should rather use the edit buffer request message to detect if a DW6000 is connected, because its reply reveals the channel.*

### Program Retrieval

Most synths (actually all vintage synths) organize their program places into banks and programs. The KnobKraft Orm does not follow that method, but rather enumerates program places in a device linearly from 0 to how many programs it can store, bank number times program number per bank. You need to do the calculation in addressing the synths bank and program on your own, and if need be also add program change and bank select messages appropriately. 

The DW6000 though is a simple example. We need to implement two functions to tell the Orm how many banks and many programs per bank the synth has:

    def numberOfBanks():

returns the number of banks. In the case of the DW6000, that is 1.

    def numberOfPatchesPerBank():

Does exactly that. In the case of the DW6000, that would be 64.

    def createProgramDumpRequest(channel, patchNo):

This function must return one or more MIDI messages in a single list of integers requesting a specific patch from the synths memory. Sometimes there is a dedicated command for it, sometimes you need to fall back to the edit buffer request. And the patchNo is the KnobKraft Orm version linearly counted, for the DW6000 this is 0..63 but for Prophet Rev2 with its 8 banks of 128 patches it will be 0..1023.

The channel handed into this function again is the channel the synth was detected at, what the function `channelIfValidDeviceResponse` returned as success.

Here is the implementation for the DW6000:

    def createProgramDumpRequest(channel, patchNo):        
        # This is done by creating a program change request and then an edit buffer request
        return [0b11000000 | channel, patchNo] + createEditBufferRequest(channel)

The 0b11000000 equals 0xc0, I use binary representation if the manual contains binary. E.g. the new Sequential documentation, which is very good, gives the values in binary. I just keep it in the program code, so I don't have to calculate.

Also important here is that we use the binary or | operator to put the channel into the first byte of the two byte MIDI program change message. The createProgramDumpRequest response here consists of two MIDI messages that will be sent to the synth, one after the other: A program change to the selected program slot, and then the result of the function we defined before, a createEditBufferRequest. Neat, isn't it?

The + operator is used in Python to concatenate two lists, forming one list out of two. Very handy.

The last function we need to implement to get the program dump mechanism working is

    def isSingleProgramDump(message):

This is very similar to the `isEditBufferDump()` described before, and actually in case of the DW6000 is implemented exactly like it with it:

    def isSingleProgramDump(message):
        # The DW-6000 does not differentiate - you need to send program change messages in between to get other programs
        return isEditBufferDump(message)

### Getting the patch's name

The database stores patches either as edit buffer dumps or as program dumps, whatever it got when the patch was downloaded. So all other functions we will write will have to deal with these two possibilities. 

One method we can implement later, when everything is ready, is the method to retrieve the patch's name. If the synth has no name memory, we will code it with a constant string anyway, but also during development it can be useful to defer the patch name extraction to later.

The signature of the method is

    def nameFromDump(message):

and it should return a string. To implement a dummy function just return a fixed string:

    def nameFromDump(message):
        # The DW-6000 has no patch name memory, so all patches get the same name for a start
        return "DW-6000"
but for complex synths you will want to extract the real name. This actually can be more involved than it appears, because some synths might have some sysex escaping algorithm, needed because sysex data is 7 bits only but the synth might want to use 8 bits for the patch data, or because it uses a character set that is not the same as the ASCII set on the computer. The Access Virus is a good example for a custom character set.

If you want to see some examples, I recommend to look at the implementation of the DSI Prophet 12, which also contains code to go from 7 bit packed sysex to 8 bit data, or the Matrix 6 Adaptation, which contains code to denibble data from 4 bit used to 8 bit data.

This is the code for the Matrix 6 `nameFromDump()`:

    def nameFromDump(message):
        if isSingleProgramDump(message):
            # To extract the name from the Matrix 6 program dump, we need to correctly de-nibble and then force the first 8 bytes into ASCII
            patchData = denibble(message, 5)
            return ''.join([chr(x if x >= 32 else x + 0x40) for x in patchData[0:8]])

The funky last return line is a Python idiom to convert a list of bytes (or integers) into a string. You can read it as "The string '', the empty string, is the separator with which to join the values of the list of characters into a text". The list of characters is created by a list comprehension over the first 8 characters in the denibbled part of the message, plus a little case switch between values lower than 32. You don't need to understand this part now, but it shows you in which depths you could end up. You have been warned (now).

### Creating the edit buffer to send

The main function of the KnobKraft Orm is obviously to send patches to audition into the synth, and we have learned that these patches are stored in the database either as edit buffer dumps or as program dumps. We always want to send edit buffer dumps, if the synth supports it, to not overwrite the synths patch memory.

There is only one function required:

    def convertToEditBuffer(channel, message):

Parameters given are `channel`, the channel detected, and the `message`, which is the patch stored in the database. The return value is a MIDI message or multiple MIDI messages (all in one list of integers) that will be sent to the synth.

In the simplest case, we just send the message as is. This is the case when the edit buffer message sent by the synth is the same as it will retrieve. Most synths - but not all - are programmed that way. 

In case of the DW6000, the operation is as simple as it can get:

    def convertToEditBuffer(channel, message):
        if isEditBufferDump(message):
            return message[0:2] + [0x30 | channel] + message[3:]
        raise Exception("This is not an edit buffer - can't be converted")

This code contains a security check in that the message handed in is actually an edit buffer dump - program errors might cause something else to end up here, and throw an exception in case this is the case. Don't worry, a regular Exception from Python will not crash the Orm, but rather create a print message in the log window.

The return statement returns the message as is, with the byte at `message[2]` again recalculated with the current MIDI channel. This code guarantees that the patches you are sending into the edit buffer will be accepted by the synth even if you have changed the MIDI channel since you sent them into the Orm. Or if you load patches from the Internet into the Orm they might be coded into a different MIDI channel than your device is setup, and adapting this in this method nicely guarantees you can switch MIDI channels without any problems!

Another example from the code for the Prophet 12 shows how to handle this when both edit buffer and program dumps are available in the database, and the synth does not care about the channel in its sysex messages. You can see that in the second case two bytes from the original message are dropped:

    def convertToEditBuffer(channel, message):
        if isEditBufferDump(message):
            return message
        elif isSingleProgramDump(message):
            # Have to strip out bank and program, and set command to edit buffer dump
            return message[0:3] + [0b00000011] + message[6:]
        raise Exception("Neither edit buffer nor program dump - can't be converted")

## Optional capabilities

Some capabilities are not required to be implemented, but enhance the user experience. 

### Renaming patches
For example, the Orm always allows the user to specify a name for a patch, but that name will not appear on the synth unless you implement the following function. If you don't implement it, the patches will keep their original name even if you change the database name for a patch.

    def renamePatch(message, new_name):

Input is a MIDI message with an edit buffer or program dump, and the new name for the patch. The method returns a MidiMessage with the renamed patch. Depending on the complexity of the data format, this can be quite an involved operation, here is an example for the Matrix 1000 (omitting the implementations of the denibble() and rebuildChecksum() methods):

    def renamePatch(message, new_name):
        if isSingleProgramDump(message) or isEditBufferDump(message):
            # The Matrix 1000 stores only 6 bit of ASCII, folding the letters into the range 0 to 31
            valid_name = [ord(x) if ord(x) < 0x60 else (ord(x) - 0x20) for x in new_name]
            new_name_nibbles = nibble([(valid_name[i] & 0x3f) if i < len(new_name) else 0x20 for i in range(8)])
            return rebuildChecksum(message[0:5] + new_name_nibbles + message[21:])
        raise Exception("Neither edit buffer nor program dump can't be converted")

You can see that it is the responsibility of this method to do a check on the name, and convert to length and character set used by that synth. If the synth is vintage, this might be more involved.

### Bank management

In order to be able to store patches not only in the edit buffer, but at a specified program location, you need to implement a method to convert the patch data into one or more messages that will store the patch at that location.

The signature is

    def convertToProgramDump(channel, message, program_number):

with the channel being the channel of the device, the message is the edit buffer or program dump that shall be converted, and the program_number is the 0-based location the store operation should be sent to.

### Better bank names

If you don't provide any special code, the banks of the synth will be just called Bank 1, Bank 2, Bank 3, ...

If you like to see the name of the Bank as the synth displays it, you can implement the method

    def friendlyBankName(bank_number):

and return a string calculated from the bank number (0 based integer) that is displayed whereever a patch location is shown in the Orm. 

As an example, a Kawai K3 adaptation might implement this as

    def friendlyBankName(bank_number):
        return "Internal memory" if bank_number == 0 else "Cartridge"
To name bank 0 as Internal Memory and all else (there will only be 1) as Cartridge.



## Examples

The KnobKraft Orm ships with quite a few examples of adaptations. 

All existing adaptations can be found in the directory with the source code, or here at github at https://github.com/christofmuc/KnobKraft-orm/tree/master/adaptions

The adaptations that are shipped with the KnobKraft Orm are compiled into the executable, so you won't find them on your disk if you install with the DMG or the installer.