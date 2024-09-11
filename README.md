<img  src="Logos/Training-Mode-banner.png"  alt=""  width="300"/>

# Training Mode - A Melee Modpack for Practicing Tech

Training Mode is a modpack for Super Smash Bros. Melee that aims to streamline practice by providing pre-made individual training scenarios. This mod utilizes the event mode present in Super Smash Bros. Melee and reworks them to focus around specific techniques, complete with automatic savestate functionality to allow for rapid-fire practice. Some events have included video tutorials playable in-game (ISO version only) to teach the player about the tech and how to perform it. In addition to these events, Training Mode also includes onscreen text displays which provide you with information otherwise unavailable to you mid-game.

## Changes
- New Training Lab Features:
    - New neutral jump option for CPUs, set as the default.
    - Jump actions no longer make the CPU SD.
    - SDI multiplier is set to low by default.
    - This is just the beginning. Much more planned in the near future!
- Features in progress:
    - Edgeguard training event.
- Developer Features:
    - Simple and easily reproducible builds on Windows and Linux.
    - Fast recompilation on Linux using make.
    - Simplified and performant [tool](https://github.com/AlexanderHarrison/gc_fst) to extract and rebuild ISOs.

## Compilation

### Windows
1. [Install DevKitPro](https://github.com/devkitPro/installer/releases/latest). Install the Gamecube (aka PPC or PowerPC) package.
2. Drag your legally obtained SSBM v1.02 ISO on to the 'build_windows.bat' file. If all goes well, 'TM-More.iso' will be created.

### Linux
1. [Install DevKitPro](https://devkitpro.org/wiki/Getting_Started#Unix-like_platforms). Install the Gamecube (gamecube-dev) package.
    - Ensure that `/opt/devkitpro/devkitPPC/bin/` is added to the PATH.
2. [Install Mono](https://www.mono-project.com/download/stable/#download-lin). Prefer installation through your package manager.
    - I would like nothing more than to remove mono, msbuild, .NET, C#, and Microsoft from my life. 
    Alas it is just as deeply ingrained the SSBM ecosystem as I am.
3. Install xdelta3. This should be simple to install through your package manager.
4. Run `make iso=path-to-melee.iso all`. If all goes well, 'TM-More.iso' will be created.
    - If the provided 'gecko' binary fails (maybe due to glibc versioning issues), you can compile your own binary from [here](https://github.com/JLaferri/gecko). YOU MUST USE VERSION 3.4.0.
    - If the provided 'gc_fst' binary fails (maybe due to glibc versioning issues), you can compile your own binary from [here](https://github.com/AlexanderHarrison/gc_fst).
