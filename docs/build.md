
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

Using CMake and building can be simple if the prerequisites are fulfilled:
* Install https://visualstudio.microsoft.com/vs/community/
* Confirm Desktop development with C++ and the MSVC C++ (latest) components are also installed
* Install latest python https://www.python.org/downloads/windows/

Simply open a command line in the downloaded root directory `<KnobKraft-orm>` and run the build using cmake:

    cmake --fresh -S . -B builds -G "Visual Studio 17 2022" -A x64

This will generate a solution file for Visual Studio in the builds subdirectory. You can build the software to run it immediately with the command

    cmake --build builds --config Release

This will produce the executable in the path `builds\The-Orm\Release`, namely a file called `KnobKraftOrm.exe` which you can double click and launch.

## Building on Linux

See the azure-pipelines.yml file for some hints how the Ubuntu server is doing it. Mainly, you need to install a long list of prerequisites and development libraries:

    sudo apt-get -y update && sudo apt-get install -y libcurl4-openssl-dev pkg-config libtbb-dev libasound2-dev libboost-dev libgtk-3-dev libwebkit2gtk-4.0-dev libglew-dev libjack-dev libicu-dev libpython3-all-dev

and then can use CMake just like on Windows to compile the software:

    cmake -D CMAKE_INTERPROCEDURAL_OPTIMIZATION=off -S . -B builds
    cmake --build builds

This will produce a single executable `builds/The-Orm/KnobKraftOrm` that you can run.

The LDFLAGS is required for a certain combination of gcc version/pybind11, else you will run into internal compiler errors. See issue #6 for a discussion.

## Building on macOS

If you are inclined to build on Mac, you know what you're doing. I'd recommend to install the build requisites via homebrew like this

    brew install gtk+3 glew boost python3 icu4c

and then run CMake to build the software

    cmake -S . -B builds/release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
    cmake --build builds/release

which will produce a launchable application in a folder called `KnobKraftOrm.app`.

Should you get an error about the required ICU libraries not being found edit the CMakeLists.txt file, where it sets
a variable called ´ICU_ROOT´.

Use this to find out the install directory of your ICU library:

    ls `brew --prefix icu4c`

The reason for the ICU making problems is that there will be an ICU already shipped with the Mac, but being incomplete. So we need
to install a complete SDK and make sure it takes precedence over the system supplied library.
