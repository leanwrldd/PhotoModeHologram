# Photo Mode Hologram

A [Satisfactory](https://www.satisfactorygame.com/) mod that keeps your hologram when you open camera mode, so you can look around it before you build.

Built with [SML](https://ficsit.app/mod/SML) (Satisfactory Mod Loader) 3.12 for Satisfactory 1.2.

## Features

- Opening camera mode no longer puts the build gun away or discards your hologram
- The hologram locks in place while camera mode is open
- Close camera mode and carry on building
- A hologram you locked yourself stays locked afterwards

Holograms the game can't lock will follow your view instead of staying in place.

## Not yet tested

This mod has not been tested in game yet, including:

- Camera mode with the free camera
- Multiplayer and dedicated servers. The mod should be installed on the host or server as well as the client.

## Building

Junction the repo into an SML project's `Mods/GameFeatures/PhotoModeHologram`, then run `Scripts/first-package.ps1`. It builds the editor target, creates the game feature asset and packages the mod with Alpakit.
