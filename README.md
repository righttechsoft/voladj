# voladj

Makes the Volume Up / Volume Down keys change the volume by 1 instead of 2.

![voladj volume indicator](voladj.gif)

## Why

On Windows, each press of a volume key moves the volume by 2. That is often too coarse: at low volume, one step is too quiet and the next is too loud. Windows has no setting to change the step.

voladj is a tiny background app that fixes this. It has no tray icon. It catches the two volume keys and moves the volume by 1 itself. Holding a key still repeats. Volume Up also turns mute off, like Windows does.

Because the keys never reach Windows, the Windows volume popup does not appear. voladj shows its own small indicator instead: a rounded bar above the taskbar, at the bottom centre of the screen. It shows a speaker icon (crossed out when muted), a level bar and the number. It follows the Windows light or dark mode, never takes focus, and lets clicks pass through. It stays for 1 second after the last key press and then fades out.

It is one small exe (about 8 KB) with no dependencies, and it uses no CPU while idle.

## Install

1. Open the [latest release](https://github.com/righttechsoft/voladj/releases/latest) and download `voladj.exe` and `install.ps1` into the same folder. Pick a folder where they can stay, for example `C:\Tools\voladj`.
2. Open PowerShell as Administrator in that folder and run:

       powershell -ExecutionPolicy Bypass -File .\install.ps1

This creates a scheduled task named `voladj` that starts the app every time you log in, and starts it right away. The task runs with highest privileges, so the keys also work while an administrator window is in focus.

If you only want to try it, just run `voladj.exe` - no install needed. It runs until you log out or stop it.

## Uninstall

From PowerShell as Administrator:

    powershell -ExecutionPolicy Bypass -File .\install.ps1 -Uninstall

Then delete the folder.

## Stop

    taskkill /im voladj.exe

If it was installed with `install.ps1`, run this from an Administrator prompt.

## Known limits

- The indicator is shown on the primary monitor only.
- The indicator is not shown for the Mute key. voladj does not handle that key, so Windows shows its own popup for it.
- Some audio devices only support coarse volume steps. On those, a step of 1 may round to the nearest step the device has.
- Some keyboards send volume through their own driver instead of the standard volume keys. Those are not caught.

## Build from source

Needs Visual Studio 2022 with the C++ tools.

    build.bat

This produces `voladj.exe`. The step size is `STEP` at the top of `voladj.c`. The indicator's size, position, hold time, fade time and opacity are the `IND_*` values next to it.

## How it works

`voladj.c` installs a low-level keyboard hook (`WH_KEYBOARD_LL`), swallows `VK_VOLUME_UP` and `VK_VOLUME_DOWN`, and sets the volume of the default playback device through `IAudioEndpointVolume`. 

For the indicator it creates one hidden, click-through, layered window at startup. On each key press it draws the bar into a 32-bit bitmap (rounded shapes with smooth edges, icon and number with GDI text) and shows it with UpdateLayeredWindow. It reads the light or dark mode from the registry each time. Two thread timers hold the bar for 1 second and then fade it out. Once it is hidden, no timer runs. The app is built without the C runtime, which keeps the exe small.

## Releases

Pushing a tag like `v1.0.0` runs `.github/workflows/release.yml`. It builds `voladj.exe` on GitHub and publishes a release with `voladj.exe` and `install.ps1` attached.

## License

MIT - see `LICENSE`.
