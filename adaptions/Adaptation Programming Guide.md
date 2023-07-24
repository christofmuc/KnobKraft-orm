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

New adaptations are stored as a single Python file with the ending `.py` in a directory on your computer, and are read in on start of the KnobKraft Orm.

The Orm will read in all python files in the specified directory, so you can name or rename the file you are working on as you like. The identity of the device supported is specified by the string returned by the `name()` function you implement, so make sure not to have two files in the directory which have the same return value there.

To find out or change the directory where those are stored, press the button `Set User Adaptation Dir` in the Setup tab for version 1.x, and in the Options menu for version 2.x.

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

# Testing

Now is a good time, before jumping right into the programming exercise, to think about how you will test that 
your adaptation code actually works. For this, I have written the 
[Adaptation Testing Guide](https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Testing%20Guide.md), 
please check it out!

# List of functions to implement

For the device to function completely within the main program, you need to implement the following list functions not marked optional. The optional functions can be implemented for additional functionality.

I list all functions in the order they appear in the example file of the Korg DW6000 adaptation, grouped by the main function they are used for.

## Identity

    def name():

This function should return a string with the name of the device, uniquely identifying it. Don't make it too long, it is used on the buttons as well!

Example implementation:

    def name():
        return "Korg DW6000"

## Storage size 

Most synths (actually all vintage synths) organize their program places into banks and programs. The KnobKraft Orm does not follow that method, but rather enumerates program places in a device linearly from 0 to how many programs it can store, bank number times program number per bank. You need to do the calculation in addressing the synths bank and program on your own, and if need be also add program change and bank select messages appropriately. 

### Old method

The DW6000 though is a simple example. We need to implement two functions to tell the Orm how many banks and many programs per bank the synth has:

    def numberOfBanks():
        return 1

returns the number of banks. In the case of the DW6000, that is 1.

    def numberOfPatchesPerBank():
        return 64

Does exactly that. In the case of the DW6000, that would be 64.

### New method

A newer way to do this is to implement the `bankDescriptors` function which gives you more control. the DW6000 would simply implement this function instead of `numberOfBanks` and `numberOfPatchesPerBank`:

    def bankDescriptors(self) -> List[Dict]

It should return a list of banks, and each bank is described by a the following fields in the Dict:

  * "bank" [int] - The number of the bank. Should be zero-based
  * "name" [str] - The friendly name of the bank
  * "size" [int] - The number of items in this bank. This allows for banks of differenct sizes for one synth
  * "type" [str] - A text describing the type of data in this bank. Could be "Patch", "Tone", "Song", "Rhythm" or whatever else is stored in banks. Will be displayed in the metadata.
  * "isROM" [bool] - Use this to indicate for later bank management functionality that the bank can be read, but not written to

Example implementation for the DW6000 replacing the two methods above:

    def bankDescriptors(self) -> List[Dict]:
        return [{"bank": 0, "name": "Internal", "size": 64, "type": "Patch"}]

more interesting would probably a more complex version for the Novation Summit which shows you how to use Python list comprehensions to calculate the 4 banks:

    def bankDescriptors():
        return [{"bank": x, "name": f"Bank {chr(ord('A')+x)}", "size": 128, "type": "Single Patch"} for x in range(4)]

### Bank Select

New with version 2 of the KnobKraft Orm, it can decide to send a bank select and program change to load a selected patch into the synth, when it knows the patch is actually already in the synth's memory via the synth bank import.

For this to work with synths that have more than 1 bank, or to be specific for those where a program change alone is not sufficient to address a specific program, another method has to be implemented to create the appropriate bank change message:

    def bankSelect(channel, bank):

This should return one or more MIDI messages that will select the bank requested. A default implementation might look like this:

    def bankSelect(channel, bank):
        return [0xb0 | (channel & 0x0f), 32, bank]

This would create a MIDI CC with controller number 32, which is used by many synth as the bank select controller.

## Device detection

The device detection mechanism needs at least two functions to be implemented to work, two more are optional.

 And note that while many devices implement a version of a device ID request message or similar, sometimes it is smarter to do something completely different to coax the MIDI channel out of the device. Or to get it to react at all, you sometimes need to use brute force. E.g. to detect a Roland MKS-80 I initiate a full bank dump, and when the first data block comes just abort the transfer. Only then I know there is an MKS-80, and the MIDI channel is encoded in its response.

 ### Sending message to force reply by device

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

### Checking if reply came

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

### Confusion about MIDI channel and "Device ID".

 Note that many synths call the individual setting its Device ID and not the MIDI channel. Think of the Device ID as a channel used only for sysex messages. The idea was if you have more than one device of the same type, you still want to be able to communicate with each device separately, so you would set them to different device IDs in their setup and then the computer. In many documents (and the Orm) this gets confused/mixed up/used interchangingly with MIDI channel, so please be aware there are subtle differences.

 Note that also many synths while allowing to specify a device ID actually will ignore it when being addressed. Many implementations are incomplete.

### Optionally specifying to not to channel specific detection

    def needsChannelSpecificDetection():

This function should return `True` if the `createDeviceDetectMessage()` should be called once for each of the 16 possible MIDI channels and MIDI outputs, or `False` if it should only be called once per MIDI output.

It is optional, if you do not implement it, it will return `True`.

### Optionally specifying a different wait time than 200 ms.

    def deviceDetectWaitMilliseconds():

You guessed it, return the number of milliseconds the main program will wait for the synth to answer before it moves on testing the next MIDI output. If this number is too low, you will get very confused because the synth will be detected on the wrong interface. Better start with a higher number, and when everything works bring this value down by experimenting. 200 ms is a good value for most devices, and is the default if you do not implement this method.

Note that the main program will only wait once for all 16 channels, if you have channel specific detection turned on. The assumption is that there is no collision between the 16 different detect messages sent, and the device will only reply to one of them. 

**[Hack that needs to be removed: return a negative number from the deviceDetectWaitMilliseconds() method, and no device detect will be attempted]**

With these four functions implemented, go ahead and test if the device detection works! Look at the MIDI log to watch the MIDI traffic. And remember that all synths need some kind of special setting/mode/switches to reply to sysex commands, better re-read that manual.

# Capabilities

When programming your Adaptation, the type of messages your device provides will also prescribe which classes of Capability or 'Device Profile' you can and should implement. Not all capability classes are to be implemented by all devices, some capabilities are a replacement for another capability in case the messages of the device function differently.

In C++, there are many more capabilities exposed, but for the Adaptations currently three different capabilities can be implemented in addition to the required functions described above and the pure optional functions described in the last section of this document.

The three capabilities are:

  1. The **Edit Buffer Capability** - this is probably the most common and intuitive capability, and most MIDI devices have the concept of an edit buffer. A transient storage of the patch that is currentl being edited by the player. Normally, a request method to retrieve the edit buffer as well as a send to edit buffer message exist. Sometimes, the request for an edit buffer is replied to with a program dump, sometimes there is a specific edit buffer dump message.
  2. The **Program Dump Capability** - this can be implemented in addition to or instead of the Edit Buffer Capability. The Program Dump capability allows to address to memory places of the synth directly via bank number/program number. Normally, there is a message to request a specific program from a specific memory position, and a message to send a patch into a specific program position. Of course, all variations and asymmetries possible exist in the MIDI world as well. In case your device has only Program Dump Capability and not Edit Buffer Capability, it is common practice to implement the Edit Buffer Capability with the help of the Program Dump Capability, but to use a fixed memory position as "edit buffer". Note that this might or might not reflect the last changes the player made to the patch. E.g. at the Kawai K3 there is no way to retrieve the transient buffer the user is currently modifying.
  3. The **Bank Dump Capability** - some synths allow to request a full bank of patches with just a single request command. The reply could then be either a single bank dump message (Kawai K3), or a stream of individual program messages (Access Virus), or a stream of program messages and other stuff that you might want to ignore (Matrix 1000). In any case, implementing the bank dump capability is more involved than the capabilities above, but usually provides much better performance. If the bank dump capability is missing, the Librarian will try to iterate over the individual patches and use either program change commands and the edit buffer requests, or the individual program dump requests. For very few devices, the bank dump capability is the only way to retrieve the content of the device.
  Some devices are able to send bank dumps but don't provide a way to request them, so implementation of the request command is optional and is deemed the **Bank Dump Request Capability**, but should only be implemented if the Bank Dump Capability is also implemented.

The capabilities are explained in the sections in more detail.

## Edit Buffer Capability

The EditBufferCapability is used to retrieve the Edit Buffer and to send a patch into the edit buffer for audition. Also, if no other capabilities are implemented and the synth reacts on program change messages, it will be used by the Librarian to retrieve, one by one, all patches from the synth.

To implement the EditBufferCapability, you need to implement three functions:

    def createEditBufferRequest(channel)
    def isEditBufferDump(message)
    def convertToEditBuffer(channel, message)

### Requesting the edit buffer from the synth

The first function to implement is this:

    def createEditBufferRequest(channel):

Similar to the `createDeviceDetectMessage`, this function should return a single MIDI message that makes the device send its Edit Buffer. In case the device doesn't support this operation, you could send it a command to return a specific program number, like the highest program slot available.

Example implementation for the DW6000:

    def createEditBufferRequest(channel):
        # Page 5 - Data Save Request
        return [0xf0, 0x42, 0x30, 0x04, 0x10, 0xf7]

You can see that I always include comments with references to the manual where I found the command, and I add the terminology used in the manual as well, because this is really different from vendor to vendor.

### Checking if a MIDI message is an edit buffer dump

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
Important here is that in the third byte index by `message[2]` I want to only check the upper 4 bits and ignore the lower 4 bits, as they contain the MIDI channel and might be any value from 0..15. So with a binary and operator `&` I mask the upper 4 bits, and compare only those.

*And yes, if you followed so far you see now that I should rather use the edit buffer request message to detect if a DW6000 is connected, because its reply reveals the channel.*

### What about edit buffer dumps that consist of more than one MIDI message?

Until Orm 1.15, the parameter message really was just a single message. But now we can deal with multi-message type synths: The parameter message basically just becomes a parameter messages, which is the same as before, a list of bytes, and the adaptation has to do the work to split into multiple MIDI messages if that is of any relevance.

The logic now works like this: An additional method `isPartOfEditBufferDump(message)` can be implemented, signaling by returning true that the message presented should be part of the messages parameter to the isEditBufferDump() message. In turn, the isEditBufferDump() should return only true if it has enough messages to complete the full edit buffer.

As an example, we can look at a typical Yamaha Sysex implementation. Let's take the Yamaha reface DX, a 4 operator FM synth which has multiple messages per edit buffer dump.

The isPartOfEditBufferDump is implemented like this:

    def isPartOfEditBufferDump(message):
        # Accept a certain set of addresses
        return isBulkHeader(message) or isBulkFooter(message) or isCommonVoice(message) or isOperator(message)

with the isBulkHeader etc functions checking this single MIDI message for certain bytes (their implementation can be found in the YamahaRefaceDX.py file in the repository.)

Now isEditBufferDump(messages) gets presented all messages which have been received where isPartOfEditBufferDump() returned true. To check that all required messages have been received and the "messages" parameter really is a full edit buffer dump, we do:

    def isEditBufferDump(data):
        messages = splitSysexMessage(data)
        headers = sum([1 if isBulkHeader(m) else 0 for m in messages])
        footers = sum([1 if isBulkFooter(m) else 0 for m in messages])
        common = sum([1 if isCommonVoice(m) else 0 for m in messages])
        operators = sum([1 if isOperator(m) else 0 for m in messages])

        return headers == 1 and footers == 1 and common == 1 and operators == 4

so basically we split the data or messages parameter into individual messages with the function presented below, and then count that we received exactly 1 header, 1 footer, 1 common and 4 operator messages. Those 7 messages together will then be stored as one patch in the Orm's database.

Here is the splitSysexMessage function used, you can also use it from the common Python package knobkraft found in the adaptation folder:

    def splitSysexMessage(messages):
        result = []
        start = 0
        read = 0
        while read < len(messages):
            if messages[read] == 0xf0:
                start = read
            elif messages[read] == 0xf7:
                result.append(messages[start:read + 1])
            read = read + 1
        return result

this expects a list of bytes, and will return a list of byte lists, with each byte list starting with 0xf0 and ending with 0xf7. bytes outside of the first 0xf0 and the last matching 0xf7 will be ignored.

### Creating the edit buffer to send

The main function of the KnobKraft Orm is obviously to send patches to audition into the synth, and we have learned that these patches are stored in the database either as edit buffer dumps or in more complex synths also e.g. as program dumps. We always want to send edit buffer dumps, if the synth supports it, to not overwrite the synths patch memory.

This is the function required:

    def convertToEditBuffer(channel, message):

Parameters given are `channel`, the channel detected, and the `message`, which is the patch stored in the database. The return value is a MIDI message or multiple MIDI messages (all in a single list of integers) that will be sent to the synth.

In the simplest case, we just send the message as is. This is the case when the edit buffer message sent by the synth is the same as it will retrieve. Most synths - but not all - are programmed that way. 

The simplest implementation you can get away is:

    def convertToEditBuffer(channel, message):
        return message

That would work for a simplistic case, but to go the full distance, in case of the DW6000, the operation is still as simple as it can get:

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

### Handshaking

Some protocols, especially those which send multiple messages per program dump or edit buffer, require a specific answer by the Librarian, either in the form of a simple ACK (acknowledge) message that signals to the synth that the previous message has been processed and the next may be sent, or even a more specific request for the next data package with the next address to deliver (multiple requests, like in the Generic Roland module).

I have hacked this possibility into the edit buffer (and program dump) capabilities in a non obvious but fairly low effort way: If you implement the `isPartOfEditBufferDump` message, do not just return a boolean to indicate if this message is part of the data or not, but a pair of a boolean and a list of bytes representing one or multiple MIDI messages that will be sent as a reply.

So for example, in case a device like the DW6000 would need an acknowledge message before it will send the next path, this could be implemented like this:

    def isPartOfEditBufferDump(message):        
        if (len(message) > 4
                and message[0] == 0xf0
                and message[1] == 0x42  # Korg
                and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
                and message[3] == 0x04  # DW-6000
                and message[4] == 0x40  # Data Dump
                ):
            return True, [0xf0, 0x42, 0x30, 0x04, 0x41, 0xf7]  # Example "0x41 ACK" message to be sent to the synth as reply
        else:
            return False

Of course this is just an example, the DW6000 has no such ACK message in real life. The same mechanism works for the isPartOfSingleProgramDump method (see below).

## Program Dump Capability


To enable the Program Dump Capability for your adaptation, which will be used instead of the Edit Buffer Capability in enumerating the patches in the synth for download, you need to implement the following three functions:

    def createProgramDumpRequest(channel, patchNo)
    def isSingleProgramDump(message)
    def convertToProgramDump(channel, message, program_number)

The third function currently is not used, but is the basis for the upcoming feature of bank mangement - to send a patch into a specific position in the synths memory, not just the edit buffer or a hard-coded "pseudo edit buffer" like slot 100. 

Additionally, when you have implement all three functions to enable the ProgramDumpCapability on the adaptation, you can also add the following optional function:

    def numberFromDump(message)

which will be used if present to detect the original program slot location stored in a program dump for archival purposes.

### Requesting a specific program at a specific memory position

The first function looks familiar:

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

### Checking if a message is a program dump 

The second function we need to implement to get the program dump mechanism working is

    def isSingleProgramDump(message):

This is very similar to the `isEditBufferDump()` described before, and actually in case of the DW6000 is implemented exactly like it with it:

    def isSingleProgramDump(message):
        # The DW-6000 does not differentiate - you need to send program change messages in between to get other programs
        return isEditBufferDump(message)

Note that the program dump capability can handle multi-MIDI message dump types exactly like the edit buffer capability (see above) by implementing a predicate function 'isPartOfSingleProgramDump'.

### Creating a message that stores a patch in a specific memory location

In order to be able to store patches not only in the edit buffer, but at a specified program location, you need to implement a method to convert the patch data into one or more messages that will store the patch at that location.

The signature is

    def convertToProgramDump(channel, message, program_number):

with the channel being the channel of the device, the message is the edit buffer or program dump that shall be converted, and the program_number is the 0-based location the store operation should be sent to.

Here is an example for the Prophet 12. Actually all Sequential/DSI synths have the same implementation for this operation. The difference in their data structure between edit buffer dump and program dump is only the operation code and that the program buffer dump has to bytes for bank and program extra:

    def convertToProgramDump(channel, message, program_number):
        bank = program_number // numberOfPatchesPerBank()
        program = program_number % numberOfPatchesPerBank()
        if isEditBufferDump(message):
            return message[0:3] + [0b00000010] + [bank, program] + message[4:]
        elif isSingleProgramDump(message):
            return message[0:3] + [0b00000010] + [bank, program] + message[6:]
        raise Exception("Neither edit buffer nor program dump - can't be converted")

## Bank Dump Capability

Some synths do no work with individual MIDI messages per patch, or even multiple MIDI messages for one patch, but rather with one big MIDI message which contains all patches of a bank. If your synth is of this type, you want to implement the following 4 functions to enable the Bank Dump Capability. Also, if you have only a single request to make, but the synth will reply with a stream of MIDI messages, this is the right capability to implement.

    def createBankDumpRequest(channel, bank)
    def isPartOfBankDump(message)
    def isBankDumpFinished(messages)
    def extractPatchesFromBank(messages)

Not all synths (e.g the Modor NF-1) allow requesting a bank dump, so in that case you can skip the first function, allowing you to receive synth-initiated dumps and load bank dump files, but no automatic bank retrieval.

### Requesting a full bank dump

You guessed it by now, you need to create a MIDI message that will make the synth send us the requested bank. Here is an example for the Korg MS2000/microKorg, where the operation is called Program Data Dump Request. We can ignore the bank parameter here, as the MS2000 effectively has only one bank:

    def createBankDumpRequest(channel, bank):        
        return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x58, 0x1c, 0xf7]

Implementing this is shown as the **Bank Dump Request Capability**, and really only works when you also implement the other methods of the Bank Dump Capability.

### Checking if a MIDI message is part of the reply

The next function will be called for all incoming MIDI messages, and should return True when the MIDI message is part of the reply and should be kept for later.

As an example, this is how the MS2000 implementation does it:

    def isPartOfBankDump(message):
        return (len(message) > 4
                and message[0] == 0xf0
                and message[1] == 0x42  # Korg
                and (message[2] & 0xf0) == 0x30
                and message[3] == 0x58  # MS2000
                and (message[4] == 0x50 or message[4] == 0x4c))  # Program Data dump or All Data dump
        
### Testing if all messages have been received

For some synths, the bank dump is only a single MIDI message, so you could just test the first message of the array given and then return True, for other synths that send a bank dump as a stream of messages you might want to implement this function to e.g. count the number of patches and decide if you have the right amount.

The implementation for the Korg MS2000 just checks if in the list of messages given at least one is part of the bank dump, and then is happy:

    def isBankDumpFinished(messages):
        for message in messages:
            if isPartOfBankDump(message):
                return True
        return False

Note that in this function, you will not get a single MIDI message or list of bytes, but rather a list of lists of bytes, i.e. a list of MIDI messages that you can iterate over.

### Extracting the patches from a bank dump

Now, this is easily the most involved function we have to build. The mission is to read a MIDI message, which we have previously identified to be part of the bank dump stream, and construct a new list of single edit buffer or program buffer messages, which can be stored separately in the database of the Librarian, and also sent into the synth for audition.

Here is the full example for the Korg MS2000, which has to unescape the sysex, i.e. restore it to 8 bit instead of the 7 bit sysex format (that function can be found in the source code of the adaptation, I don't list it here), and with the extracted partial data it will construct a new edit buffer for each patch loaded, and append it into the result list. The return value is again one list of bytes, but it will contain many MIDI messages one after each other. Each MIDI message returned is an edit buffer dump message for the Korg MS2000.

    def extractPatchesFromBank(message):
        if isPartOfBankDump(message):
            channel = message[2] & 0x0f
            data = unescapeSysex(message[5:-1])
            # There are different files out there with different number of patches (64 or 128), plus the global data
            data_pointer = 0
            result = []
            while data_pointer + 254 < len(data):
                # Read one more patch
                next_patch = data[data_pointer:data_pointer + 254]
                next_program_dump = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x58, 0x40] + escapeSysex(next_patch) + [0xf7]
                print("Found patch " + nameFromDump(next_program_dump))
                result = result + next_program_dump
                data_pointer = data_pointer + 254
            return result
        raise Exception("This code can only read a single message of type 'ALL DATA DUMP'")

The `extractPatchesFromBank` works when each single MIDI message contains one or more patches, and each patch can be represented by a single MIDI message.
This is true for many very old synths, but newer synths with more complex data structures might require an even more complex function.

Instead of `extractPatchesFromBank`, we can implement the `extractPatchesFromAllBankMessages`. If this is defined, any implementation of extractPatchesFromBank 
is ignored.

The signature of the `extractPatchesFromAllBankMessages` is this:

    def extractPatchesFromAllBankMessages(messages: List[List[byte]]) -> List[List[byte]]:

As a parameter, it gets a list of MIDI messages. Each MIDI message is in itself a list of bytes.
The return value is a list of patches. Each patch is another list of bytes. BUT in the returned list of bytes there might be multiple
MIDI messages concatenated to each other!

Rewriting the function above for the new interface we get this:

    def extractPatchesFromAllBankMessages(messages):
        all_patches = []
        for message in messages:
            channel = message[2] & 0x0f
            data = unescapeSysex(message[5:-1])
            # There are different files out there with different number of patches (64 or 128), plus the global data
            data_pointer = 0
            result = []
            while data_pointer + 254 < len(data):
                # Read one more patch
                next_patch = data[data_pointer:data_pointer + 254]
                next_program_dump = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x58, 0x40] + escapeSysex(next_patch) + [0xf7]
                print("Found patch " + nameFromDump(next_program_dump))
                result = result + next_program_dump
                data_pointer = data_pointer + 254
            all_patches.append(result)
        return all_patches

For a way more complex example, have a look at the implementation in the Roland MKS-70 V4 adaptation.

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

If you want to see some examples, I recommend to look at the implementation of the DSI Prophet 12, which also contains code to go from 7 bit packed sysex to 8-bit data, or the Matrix 6 Adaptation, which contains code to denibble data from 4 bit used to 8-bit data.

This is the code for the Matrix 6 `nameFromDump()`:

    def nameFromDump(message):
        if isSingleProgramDump(message):
            # To extract the name from the Matrix 6 program dump, we need to correctly de-nibble and then force the first 8 bytes into ASCII
            patchData = denibble(message, 5)
            return ''.join([chr(x if x >= 32 else x + 0x40) for x in patchData[0:8]])

The funky last return line is a Python idiom to convert a list of bytes (or integers) into a string. You can read it as "The string '', the empty string, is the separator with which to join the values of the list of characters into a text". The list of characters is created by a list comprehension over the first 8 characters in the denibbled part of the message, plus a little case switch between values lower than 32. You don't need to understand this part now, but it shows you in which depths you could end up. You have been warned (now).

# Optional capabilities

Some capabilities are not required to be implemented, but enhance the user experience. 

## Throttling communication

Some older devices don't like it if multiple messages are sent to them too quickly, their small processors need a while to finish with message received and they might just ignore another message if it comes up too quickly. Actually some older devices are really good and fast despite running on some 2 MHz micro-processor like the Zilog Z80, but many also like it if there is a delay between messages.

To turn on a delay between messages sent, implement the following function returning an integer milliseconds value:

    def generalMessageDelay():

This delay will be used only in those cases where a method returns multiple MIDI messages, not between calls to methods. E.g. if the `createProgramDumpRequest` returns an array which contains two messages, a program change and an edit buffer request message, the Orm will wait the specified milliseconds after sending the first message before sending the second. It will not wait before sending the first message.

## Renaming patches
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

## Better handling of given names vs default names

Some synths make naming hard. Mainly they fall into two categories:

  1. Modern synths have a name (or multiple tones, layers, ...) stored as part of the patch. Those will want to implement the function `renamePatch` to make sure when you rename a patch in the database and you send it to the synth, you will also see the new name displayed in the synth. See above for that.
  2. Older synths (mostly vintage) do not store a name as part of the patch. Those still can get nice names in the database and displayed in the Orm, but have no name as part of the data.

For category two, we can run into problems when reimporting already known patches, either because we reimport a sysex file by accident or the patches are retrieved again from the synth.

When a patch is encountered again, the database tries to keep "the better name". By default, it will just take the newer name. But if you have renamed a patch, you might want to keep the new name.

To teach the database logic what is a better name, you can implement the following function:

    def isDefaultName(patchName):

When this returns True for a given string, this name is considered "Default", meaning without human intervention, and is given less priority when another name for the same patch is encountered.

For case number 1 above, here is an example. The OB-6, which does store patch names in the patch, but has no display to show them (yay!), might implement this simply as:

    def isDefaultName(patchName):
        return patchName == "Basic Program"

Which would prioritize the default name (the init program's name on that synth) lower than any other string.

For case number 2, let's look at the implementation of this function for the Oberheim OB-8. The OB-8 has no patch storage, but stores a program number as part of the patch (it has no edit buffer). Therefore, we can generate a nice patch name from the stored number. Simplified, the OB-8 implementation for nameFromDump() looks like this:

    def nameFromDump(message):
        if isSingleProgramDump(message):
            program = numberFromDump(message)            
            return "OB-8: %s" % friendlyProgramName(program)

And this will produce strings like `OB8: AB9` or `OB8: ACD7`. Now to detect all of the possibly generated names and decide whether they are default or not becomes more complex, and we will use the regular expression library from Python to set up a general detector to decide if the name given was actually generated by the above function (needs an `import re` statement at the top of the file):

    def isDefaultName(patchName):
        return re.match("OB-8: [A-D]*[1-8]", patchName) is not None
        
If this looks scary to you, then probably only because you have not used regular expressions before. They are a very powerful and useful tool to master, so attention is encouraged.


## Better duplicate detection

The Orm will by default check all bytes from the MIDI message for a patch to see if it has a duplicate in the database. But most often, not all bytes are relevant to check for duplicates. Much to the contrary, you probably want to prevent a duplicate from being created just because the patch data contains the program place or bank number, or just the name changed but the sound-relevant patch data is still the same.

To enable better duplicate detection, you need to implement a single optional function that takes a patch MIDI message as input, and shall return a unique key calculated from the relevant bytes of the patch.

What sounds complex can be actually quite simple in Python, for example to calculate the so called fingerprint for a patch for the Prophet 12 synth, this is how you can do it (slightly simplified version here):

    def calculateFingerprint(message):
        data = unescapeSysex(message[6:-1])
        # Blank out Layer A and Layer B name, they should not matter for the fingerprint
        data[402:402 + name_len] = [0] * name_len
        data[914:914 + name_len] = [0] * name_len  # each layer needs 512 bytes
    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data

So the heavy lifting is done by Python's hashlib, make sure to have an `import hashlib` statement at the beginning of the adaptation file (yes, you can import libraries from Python!). We use the md5 hash function and generate a hexdigest human readable string.

The important bit here is that we strip away the bank and program info by just using the message bytes from index 6 onwards and ignoring the last byte 0xf7, and by blanking out the names of layer A and layer B before calculating the fingerprint, so a changed name does not change the fingerprint for this patch.

## Better bank names

If you don't provide any special code, the banks of the synth will be just called Bank 1, Bank 2, Bank 3, ...

If you like to see the name of the Bank as the synth displays it, you can implement the method

    def friendlyBankName(bank_number):

and return a string calculated from the bank number (0 based integer) that is displayed whereever a patch location is shown in the Orm. 

As an example, a Kawai K3 adaptation might implement this as

    def friendlyBankName(bank_number):
        return "Internal memory" if bank_number == 0 else "Cartridge"
To name bank 0 as Internal Memory and all else (there will only be 1) as Cartridge.

## Better program number names

By default, we just count through all patches starting from zero until the highest number, which would be number of banks times number of patches per bank. But if you want to match the display of the memory slot to match the naming on the synth, implement this function to create a name matching the synth's terminology:

    def friendlyProgramName(program):

For example, the good old Korg DW8000 has 64 slots, but has banks 1 to 8 and in each bank it will have programs 1 to 8. The default implementation would call the program slots 0 to 63, but to make them appear as 11..18 21..28 ... 81..88 like on the device, implement this:

    def friendlyProgramName(program):
        return "%d%d" % (program // 8 + 1, (program % 8) + 1)

## Exposing layers/splits of a patch

Many synths have more than one "tone" or "layer", and some might even offer different layer modes like split or stack. And most will have more than one name for the while sound, e.g. many DSI synths have layers and will actually not name the whole patch, but rather each layer individually.

To expose this information, there is a capability called "LayeredPatchCapability", and in order to utilize this we must implement at least two functions:

    def numberOfLayers(messages):

inspect the patch/data given, and return the number of layers in it. Most often than not, this is not dynamic but rather a fixed architecture of the synth. So the implementation for the DSI Prophet 12 would look like this:

    def numberOfLayers(messages):
        return 2

Secondly, you need to implement the function that returns the name of a given layer (numbered starting with layer 0):

    def layerName(messages, layerNo):

Again, process the message(s) given and return a string with the layer name.

Optionally, if you want to allow the user to change a layer name, implement the function to set the layer's name like this:

    def setLayerName(self, messages, layerNo, new_name):

This work like renamePatch, just with the added layerNo parameter.

## Importing categories stored in the patch in the synth

Some synths store sound categories or tags. As the KnobKraft Orm has these as a central part of the UI, of course we want to be able to import them as well. It is easy enough and requires two steps: Implementing a function and then defining an import mapping.

The function that needs definition is 

    def storedTags(self, message) -> List[str]:

Just return a list of strings that map to the categories defined im the KnobKraft database. In case you want to be more flexible, you can also return the original manufacturer's texts and use the import mapping feature. For that, use the menu item "Categories... Edit category import mapping" in the Orm itself, and edit the jsonc file to define a mapping from the strings your adaptation returns. Here is am example mapping:

    {
        {
        "Roland XV-3080": {
            "synthToDatabase": {
                "EL. PIANO": "Keys",
                "MALLET": "Keys"
            }
        }
    }

You don't have to do a complete mapping - look at the log windoow after import to see if any extracted categories have been ignored because of a missing mapping. And yes, it is always safe to reimport.

## Leaving helpful setup information specific for a synth

Especially some of our more vintage synths require some preset done, sometimes after every power on, before they can be accessed by the KnobKraft Orm. You can implement the following optional function to return a text displayed to the user in the synth's settings tab:

    def setupHelp():

This just returns a text string displayed. Make sure to quote a possible multiline text correctly. The return character has to be encoded as `\n`, and as it is Python you need a trailing backslash if the line continues into the next line. Or just copy this example and change the text:

    def setupHelp():
        return "The DSI Prophet 12 has two relevant global settings:\n\n" \
            "1. You must set MIDI Sysex Enable to On\n" \
            "2. You must choose the MIDI Sysex Cable.\n\n" \
            "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
            "Both settings are accessible via the GLOBALS menu."

## Examples

The KnobKraft Orm ships with quite a few examples of adaptations. 

All existing adaptations can be found in the directory with the source code, or here at github at 

https://github.com/christofmuc/KnobKraft-orm/tree/master/adaptions

To give you a quick overview which adaptation actually has implemented which functions and capabilities, there is am overview table that you can consult, especially for finding one of the more obscure function usages:

https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/implementation_overview.md


The adaptations that are shipped with the KnobKraft Orm are stored in the adaptations subdirectory of the program directory. Check them out there as well.