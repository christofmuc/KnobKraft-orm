# Sequential (Dave Smith Instruments) OB-6 sysex implementation

This is C++ code used by the free Sysx Librarian [KnobKraft Orm](https://github.com/christofmuc/KnobKraft-orm) to talk to the synthesizer and pull and send patches from the device.

## Introduction to MidiKraft

MidiKraft is C++ for writing software like editors and librarians to interface specific hardware MIDI devices.

MidiKraft are a set of base libraries to provide more helper classes to the awesome [JUCE](https://github.com/juce-framework/JUCE) framework useful when working with real life hardware MIDI synthesizers, and also to provide implementations for the individual synthesizers to be used in other programs.

 This is the current list of MidiKraft libraries available on github:

  * [MidiKraft-base](https://github.com/christofmuc/MidiKraft-base): Base library used by all others providing some common definitions
  * [MidiKraft-librarian](https://github.com/christofmuc/MidiKraft-librarian): Implementation of various different handshake and communication methods to retrieve and send data between the host computer and a MIDI device
  * [MidiKraft-database](https://github.com/christofmuc/MidiKraft-database): Code to store MIDI data in a SQLite database

## MidiKraft Synthesizer implementations

We are working on a range of hardware synthesizer and other MIDI device implementations, to provide source code to utilize these devices to their fullest potential. Checkout our repos.
  
## Usage

This MidiKraft repository is meant to be included as a git submodule in a main project, checkout the KnobKraft Orm repository to see how it is used.

## Licensing

As some substantial work has gone into the development of this and related software, I decided to offer a dual license - AGPL, see the LICENSE.md file for the details, for everybody interested in how this works and willing to spend some time her- or himself on this, and a commercial MIT license available from me on request. Thus I can help the OpenSource community without blocking possible commercial applications.

## Contributing

All pull requests and issues welcome, I will try to get back to you as soon as I can. Due to the dual licensing please be aware that I will need to request transfer of copyright on accepting a PR. 

## About the author

Christof is a lifelong software developer having worked in various industries, and can't stop his programming hobby anyway. 
