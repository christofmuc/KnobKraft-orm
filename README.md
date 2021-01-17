# A free MIDI Sysex Librarian - The KnobKraft Orm

[![release](https://img.shields.io/github/v/release/christofmuc/KnobKraft-orm?style=plastic)](https://github.com/christofmuc/KnobKraft-orm/releases)
[![Build Status](https://dev.azure.com/christof0759/KnobKraft/_apis/build/status/christofmuc.KnobKraft-orm?branchName=master)](https://dev.azure.com/christof0759/KnobKraft/_build/latest?definitionId=1&branchName=master)

If you are looking for a modern, free Sysex Librarian for your synth, you have found the right place! This is the place where the KnobKraft Orm is created, a modern cross-platform Sysex Librarian for your MIDI gear.

Questions and help with implementing new synths wanted! Or if you have found a bug, also feel free to report directly here on Github. 


| Manufacturer  | Synth | Status | Type | Kudos |
| ------------- | ------------- | --- | --- | --- |
| Access  | Virus A, B, Classic, KB, Indigo  | works | native | |
| Access  | Virus C  | beta | native | Thanks to guavadude@gs |
| Alesis | Andromeda A6 | works | adaptation | Thanks to @markusschloesser |
| Behringer | BCR2000 | in progess | native | |
| Behringer | Deepmind 12 | alpha | adaptation | |
| Behringer | RD-8 | in progress | native | |
| Black Corporation | Kijimi | in progress | adaptation | Thanks to @ffont|
| DSI | Pro 2 | beta | adaptation | |
| DSI | Prophet 8 | beta | adaptation | |
| DSI/Sequential | OB-6 | works | native | |
| DSI/Sequential | Prophet Rev2 | works | native | |
| DSI/Sequential | Prophet 12 | works | adaptation | Thanks to @Andy2No |
| Electra | one | works | adaptation |
| Kawai | K1/K1m/K1r | beta | adaptation | | 
| Kawai | K3/K3m | works | native | | 
| Korg | DW-6000 | beta | adaptation | |
| Korg | DW-8000/EX-8000 | works | native | |
| Korg | MS2000/microKORG | beta | adaptation | Thanks to @windo|
| Oberheim | Matrix 6/6R | works | adaptation | Thanks to @tsantilis |
| Oberheim | Matrix 1000 | works | native | |
| Oberheim | OB-X (Encore) | alpha | adaptation | |
| Oberheim | OB-Xa (Encore) | alpha | adaptation | |
| Oberheim | OB-8 | beta | adaptation | |
| Pioneer | Toraiz AS-1 | beta | adaptation | |
| Quasimidi | Cyber-6 | alpha | adaptation | |
| Roland | JX-8P | alpha | adaptation | |
| Roland | D-50 | in progress | adaptation | |
| Roland | MKS-50 | in progress | native | |
| Roland | MKS-80 | in progress | native | |
| Sequential| Pro 3 | works | adaptation | |
| Sequential | Prophet 5 Rev 4 | works | adaptation | |
| Sequential | Prophet 6 | beta | adaptation | |
| Sequential | Prophet X | works | adaptation | |
| Waldorf | Blofeld | alpha | adaptation | |
| Yamaha | reface DX | works | native | |

Please get back to me if you encounter any issues, or also if you successfully test those marked as alpha or beta. The ones "in progress" are already nearly done and not part of the regular build yet, drop me a note if you want to accelerate.

The adaptations are python scripts for a generic module that let's you hook up other MIDI gear yourself, much in the spirit of the good old SoundDiver adaptations - at least, scripting the MIDI. No custom UI currently is possible or planned as of now.

So basically everyone who can read the MIDI spec and can do a little scripting could create new adaptations for more devices!

# How does it look?

[Disclaimer]: The video shows version 1.0.0, a lot has happened since then.

I made a video to show you the software and the most basic functionality, checkout the YouTube channel for more examples and advanced features as well

[![](youtube-screenshot.PNG)](https://youtu.be/lPoFOVpTANM)

# Downloading the software

I provide installer builds for Windows and disk images for macOS, they are hosted here in github. To install, just grab the latest installer executable or DMG file from the following page and run it. The Mac version needs to be installed (drag into application folder after opening the DMG) and then run with the CTRL-Click Open command (to ignore unknown source warning, as I do not sign the installers with a certificate).

[https://github.com/christofmuc/KnobKraft-orm/releases](https://github.com/christofmuc/KnobKraft-orm/releases)

Linux is reported to build and run as well, but due to the multitude of possible installations I suggest you follow the build instructions below, it shouldn't be too hard.

You can always use the source to build it yourself, please read on for more instructions.

## Supported platforms

This software is build and run on Windows 10, macOS 10.15, and several Linux distributions. Note that this is not a commercial project, and as I am using Windows mostly expect some hiccups. But I will get back to you if you report a bug and try to resolve it!

# Building your own adaptation for a synthesizer

It is possible to create an adaptation for a new synthesizer that is not yet on the supported device list. For that, you'll select an existing adaptation that might be close to what you need (e.g. same manufacturer, same device family), and use a text editor to adapt the Python code controlling how to generate the device specific messages required and what to do with the answers from the synth.

If you're up to that, I have written a whole [Programming Guide](https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md) documenting the required and optional methods to be implemented.

I know this is not easy, and most importantly new devices also might require capabilities that are not yet part of the Orm, so please don't hesitate to contact me, I'll try to help!

# Building the software

## Prerequisites

We use [CMake 3.18](https://cmake.org/). Make sure to have a recent version of cmake installed, the build process fails if it is too old (normally it will say so).

## Downloading

Clone with submodules from github

    git clone --recurse-submodules https://github.com/christofmuc/KnobKraft-orm

The recursive clone with  submodules is required to retrieve the following additional modules already into the right spot inside the source tree:

1. We use the magnificent [JUCE library](https://juce.com/) to immensly reduce the amount of work we have to do. 

4. The configure step will download (on Windows) the almighty [boost](https://www.boost.org/) library, sorry for the bloat but I simply had no time to remove the dependency yet. All my professional projects of course rely on boost, so it is a natural to incorporate it here as well.

## Building on Windows

Using CMake and building is a simple step if the prerequisites are fulfilled. Simply open a command line in the downloaded root directory `<KnobKraft-orm>` and run

    cmake -S . -B builds -G "Visual Studio 15 2017 Win64"

This will generate a solution file for Visual Studio in the builds subdirctory. You can build the software to run it immediately with the command

    cmake --build builds --config Release

This will produce the executable in the path `builds\The-Orm\Release`, namely a file called `KnobKraftOrm.exe` which you can double click and launch.

## Building on Linux

See the azure-pipelines.yml file for some hints how the Ubuntu server is doing it. Mainly, you need to install a long list of prerequisites and development libraries:

    sudo apt-get -y update && sudo apt-get install -y libcurl4-openssl-dev pkg-config libtbb-dev libasound2-dev libboost-dev libgtk-3-dev libwebkit2gtk-4.0-dev libglew-dev libjack-dev libicu-dev libpython3-all-dev

and then can use CMake just like on Windows to compile the software:

    cmake -S . -B builds && cmake --build builds

This will produce a single executable `builds/The-Orm/KnobKraftOrm` that you can run.

## Building on macOS

If you are inclined to build on Mac, you know what you're doing. I'd recommend to install the build requisites via homebrew like this

    brew install gtk+3 glew boost python3

and then run CMake to build the software

    cmake -S . -B builds/release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
    cmake --build builds/release 

which will produce a launchable application in a folder called `KnobKraftOrm.app`.
    

## Licensing

As some substantial work has gone into the development of this, I decided to offer a dual license - AGPL, see the LICENSE.md file for the details, for everybody interested in how this works and willing to spend some time her- or himself on this, and a commercial MIT license available from me on request. Thus I can help the OpenSource community without blocking possible commercial applications.

## Contributing

All pull requests and issues welcome, I will try to get back to you as soon as I can. Due to the dual licensing please be aware that I will need to request transfer of copyright on accepting a PR. 

## Acknowledgements

Really big thanks to everybody who contributes and comments on YouTube, here on Github, or any of the other forums out in the Internet to motivate me to continue this work!

Special thanks to @tsantilis for getting the Oberheim Matrix 6/6R adaption to work with restless tests and trials!

The app icon is courtesy of W07 at the Sequential forums, thanks for your contribution!

For the restless Prophet 12 testing thanks and a medal for most comments on a ticket go to @Andy2No!

Thanks also go to @gnidorah and @dukzcry for reporting bugs with the RefaceDX implementation, which were fixed! Thanks to @GriffReborn for bug reports with the Virus B implementation! Big shout out over to gearslutz' @Behrmoog, who did the first three synth adaptations on his own, brave and fearless! Special thanks to @windo who spent nights testing the Korg MS2000 to make it work, despite me being too stupid to understand a quite clear sysex documentation! Also many thanks to @markusschloesser for great feedback and inquisitive persistence! 

For bug reports, thanks to Iulian from the Facebook M1000 group!

## Prior Art

This is by far not the first attempt at solving the challenge, I only hope it is the last and this time for good with the help of the community. These are similar projects which I found during my wanderings through the net, some of these still available, some nearly lost in the mist of time. In no particular order:

| Name  | OpenSource | OS | Languange | Looks like it started | Driver-Design | User-extensible |
| ------------- | ------------- | --- | --- | --- | -- | -- | 
| eMagic's SoundDiver |No | Win, Mac | ? | 1995 | Data-driven | Yes |
| [Universal Manager](https://www.nilsschneider.de/wp/2020/06/28/universal-manager-updated-after-15-years/) |  No | Win32 | ? | 2005 | Data-driven | Yes |
| [JSynthLib](http://www.jsynthlib.org/) | Yes | Win, Linux | Java |    1999 | [Java code](https://sourceforge.net/p/jsynthlib/jsynthlib/ci/master/tree/) | Yes |
| [SyxLibEd](http://www.october28.com/syxlibed_home.asp) | No | Win32 | VisualBasic | 1997 | Database | Yes |
| [EdiSyn](https://github.com/eclab/edisyn) | Yes | Win, Mac, Linux | Java | 2017 | Java code | Yes |
| [CTRLR](https://github.com/RomanKubiak/ctrlr) | Yes | Win, Mac | C++ & Lua | 2015 | Lua code | Yes |
| [LaserMammoth](https://f0f7.net/fe/#/SysexLibrarian) | Yes | Chrome Web | Javascript & PHP | 2016 | Javascript code | Yes |
| [MidiManager](https://midimanager.com/) | No | Chrome Web | ? | 2020 | JSON data | Yes |
|  [Opcode Systems Galaxy](http://web.archive.org/web/19961113040000/http://www.opcode.com/products/gal_gpe/) | No| Mac | ? | 1990 | ? | Yes |
| [MidiSynth](http://www.sigabort.co/midisynth.html) | No | Win, Mac | ? | 2015 | ? | No |
| [Unisynth](http://www.midimetric.com/home.html) | No | Win | C# .net | 1999 | ? | No |
| [Patch Base](https://coffeeshopped.com/patch-base) | No | iOS, Mac | ? | 2020 | ? | No |
| [ToneTweak](https://tonetweak.com/sysex-librarian) | No | Chrome Web | ? | 2020 | ? | No |
| [MidiQuest](https://squest.com/) | No | Win, Mac | ? | 2008 | ? | No |

    SoundDiver I think is the one to rule them all, its legacy is what makes me do this.

## About the author

Christof is a lifelong software developer having worked in various industries, and can't stop his programming hobby anyway. 
