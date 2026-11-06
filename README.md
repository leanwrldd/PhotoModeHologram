# Photo Mode Hologram

A [Satisfactory](https://www.satisfactorygame.com/) mod that keeps your hologram when you open camera mode, so you can look around it before you build.

Built with [SML](https://ficsit.app/mod/SML) (Satisfactory Mod Loader) 3.12 for Satisfactory 1.2.

## Features

- Opening camera mode no longer puts the build gun away or discards your hologram
- The hologram locks in place while camera mode is open
- Close camera mode and carry on building
- A hologram you locked yourself stays locked afterwards

Holograms the game can't lock will follow your view instead of staying in place.

## Smart Camera

[Smart Camera](https://ficsit.app/mod/SmartCamera) is optional. With version 2.0 installed:

- Close camera mode and the build viewport shows where you left the free camera, so you can keep an eye on that spot while you build
- The view stays while you place pieces and switch recipes
- Put the build gun away or switch to dismantle, and the normal build preview comes back

The view appears in Smart Camera's settings as **Camera Mode View**. You can move and resize it with F8, separately from the build preview. Other Smart Camera versions turn this feature off, and the rest of the mod keeps working.

## Smart!

Smart! clears its grid and extend previews when camera mode opens. Keeping them needs a Smart! build with the camera mode fix, which isn't in a published release yet.

## Not yet tested

Tested in single player. Multiplayer and dedicated servers have not been tested. The mod should be installed on the host or server as well as the client.

## Building

Junction the repo into an SML project's `Mods/GameFeatures/PhotoModeHologram`, then run `Scripts/first-package.ps1`. It builds the editor target, creates the game feature asset and packages the mod with Alpakit.
