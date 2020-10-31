# Swarm SDK Template

The Swarm SDK Template implements the Orange Box SDK Template into Alien Swarm, and nothing more. It is the ideal swarm-free base from which to build your own new game.

This mod only requires swarm_base! (Meaning you can make a true mod using Alien Swarm).

## Usage:

**game/** includes the compiled build and assets for your new mod. Copy it to `steam/steamapps/sourcemods/mod_sdk`

Launch the mod with any of the 3 .bat files:

- run_mod.bat
- run_mod_debug.bat
- run_mod_tools.bat

## Compiling:

**src/** includes the full source code. Clone it anywhere you want.

Open `creategameprojects.bat` let it generate the .snl file to get started.

## Requirements:

Alien Swarm (free):
http://store.steampowered.com/app/630/

Microsoft Visual Studio 2013 Community With Update 5:
https://visualstudio.microsoft.com/vs/older-downloads/

## New Features:

### 10/30/2020:

* The old GameUI from 2007/2013 is back.

### Originally: 

* Added shader source code.
* The old 'Create a game' VGUI panel is back.
* Player list VGUI panel is back (Mute players).
* Added cl_righthand.
* No longer needs swarmframescheme.res, swarmscheme.res or swarmserverbrowserscheme.res.
* Added a credits page VGUI panel.

## Fixes:

- Fixed the color scheme for the dedicated server browser.
- Fixed voice communication (now it works!).
- Fixed close to all errors in the console.
- Fixed weapon animations on player models.
- Fixed VGUI panel: Mouse Settings.
- Fixed bullet decals not showing.
- Fixed "drop" convar not working (drop active weapon).
- Fixed team menu not popping up after MOTD.
- Fixed "fov_desired" convar missing.
- Removed leftovers in the code from the mod 'Somme'.
- Fixed ERRORNAME in death notices. (Thanks to DaFox)
- Fixed no crosshair being displayed. (Thanks to Ben Pye and Sandern)
- Fixed no voice icon being displayed on HUD. (Thanks to Ben Pye)
- Fixed no visible mouse pointer when not inside any vgui panel. (Thanks to Cpt.Semmel)
- Fixed MOTD not showing the contents of motd.txt. (Thanks to Cpt.Semmel)

## Improvements:

- New main menu.
- New scoreboard.
- New keyboard layout.
- Main menu music is now sound/ui/gamestartup1.wav again.
- English localization files revamped.
- File structure revamped.
- Movement cvars unlocked (sv_accelerate, sv_friction etc.).
- All asw_* and tilegen_* commands are now disabled.
- Added swarmsdktemplate.fgd.
- Added Steam and application icon.

## Remaining Bugs:

* VGUI model panels not in correct position on player class selection.

Improved version of http://www.moddb.com/mods/alien-swarm-sdk-template-conversion

Thanks to, Tony Sergi, DaFox, Cpt.Semmel, c_b_fofep, Doplhur and Valve.
