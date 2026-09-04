# Download KnobKraft Orm

KnobKraft Orm is free and open source. Get official builds from [GitHub Releases](https://github.com/christofmuc/KnobKraft-orm/releases/latest).

**Latest release checked: 2.10.0, published 4 September 2026.** Check the release notes and assets for the build you install; newer source may include synth support not yet in the published installers.

| Platform | Release 2.10.0 download | Install |
| --- | --- | --- |
| Windows | [Windows installer (.exe)](https://github.com/christofmuc/KnobKraft-orm/releases/download/2.10.0/knobkraft_orm_setup_2.10.0.exe) | Run the installer, then launch KnobKraft Orm. |
| macOS | [macOS disk image (.dmg)](https://github.com/christofmuc/KnobKraft-orm/releases/download/2.10.0/KnobKraft_Orm-2.10.0-Darwin.dmg) | Open the image and drag the app into Applications. The DMG includes Python. |
| Linux | [Linux archive](https://github.com/christofmuc/KnobKraft-orm/releases/download/2.10.0/KnobKraft_Orm-2.10.0-Linux.tar.gz) · [Ubuntu 24 archive](https://github.com/christofmuc/KnobKraft-orm/releases/download/2.10.0/KnobKraft_Orm-2.10.0-Ubuntu24.tar.gz) | Extract the matching archive. Linux binaries depend on distribution libraries; use the release notes or build for your distribution if they do not run. |

[Read the 2.10.0 release notes](https://github.com/christofmuc/KnobKraft-orm/releases/tag/2.10.0). For other platforms and toolchains, see [Build from source](build.md).

## Installing on Linux with Nix

KnobKraft Orm is available in [Nixpkgs](https://github.com/NixOS/nixpkgs/tree/master/pkgs/by-name/kn/knobkraft-orm), thanks to package maintainer [@backtail](https://github.com/backtail). Nix can be used on Ubuntu and other Linux distributions as well as NixOS; switching your operating system is not necessary.

After [installing the Nix package manager](https://nixos.org/download/), try Orm with:

```sh
nix --extra-experimental-features 'nix-command flakes' run nixpkgs#knobkraft-orm
```

Nix provides the package's dependencies and downloads prebuilt packages when available, or builds them from source. The packaged version depends on your selected Nixpkgs revision and may lag behind our latest release.

This is a possible route for Ubuntu users whose downloaded release binary has incompatible system libraries. Orm's GUI and MIDI operation through Nix on Ubuntu have not yet been verified by the project. Graphics drivers may need additional integration such as [nixGL](https://github.com/nix-community/nixGL), and MIDI devices must be accessible to your Linux user. Please report your distribution, package version and MIDI interface when sharing results.

## First launch

If macOS blocks the downloaded app, follow the operating system's instructions for opening a trusted downloaded application. Older Ctrl-click and Python/Homebrew advice can depend on the macOS and KnobKraft version; it is not a prerequisite to install an old Python for the current DMG.

On any platform, report a launch problem with your exact operating system, processor and app version in [GitHub issues](https://github.com/christofmuc/KnobKraft-orm/issues).

## Connect and import

Continue with [your first session](learn/index.md). You can import compatible files and organise a library without connected hardware. To hear patches, connect the matching synth and monitor its audio output.

## Updating an existing library

Before updating, keep a separate [database copy](manual/07-export-and-backups.md#save-a-database-copy), along with custom adaptations and settings. A new app version may migrate a database; retain the original copy if you need to return to an older version.
