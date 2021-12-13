# Bluetooth Low Energy (BLE) pedal for the PG Spark Amp

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/ea220b14059e479ab6a0419a1c4935f8)](https://www.codacy.com/gh/copych/BLE_Spark_pedal/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=copych/BLE_Spark_pedal&amp;utm_campaign=Badge_Grade)
[![](https://www.travis-ci.com/copych/BLE_Spark_pedal.svg?branch=withBLE)](https://www.travis-ci.com/github/copych/BLE_Spark_pedal)


This is my version of copych's BLE_Spark_pedal. I don't know coding, just copy-and-paste to modified some functions to convert it into 4-buttons Pedal:

   Common Function:

      Long Press Button:

         1: Hardware Mode

         2: Software Bank Mode

         3: Pedal Mode

         4: Sleep
         
         patch 5 is skipped since it is for hw temp


Hardware Mode (Preset 1-4)

      Long Press Button:

         1: Bypass

       Click Button:

         1: HW preset 1

         2: HW preset 2

         3: HW preset 3

         4: HW preset 4
      

Software Bank Mode (Preset 6-59)

      Long Press Button:

         2: Bypass

       Click Button:

         1: Preset Up

         2: Preset Down

         3: Toggle Drive(default)/Modulation

         4: Toggle Delay(default)/Reverb

        Double Click Button:

         1: Preset Up +10 and round down to *0. if current preset is X9, it effect as +20 and round down. (no point double clicking if you are at X9)
            e.g., 22 -> 30; 29-> 40

         2: Preset Down +10 and round down to *0. if current preset is X8/X9, it round down to X1 instead of X0-10.
            e.g., 22 -> 10; 28 -> 21

         3: switch click button 3 behavior

         4: switch click button 4 behavior
      

Pedal Mode

      Long Press Button:

         3: Bypass

       Click Button:

         1: Toggle Drive

         2: Toggle Modulation

         3: Toggle Delay

         4: Toggle Reverb

       Double Click Button:

         1: Preset Up

         2: Preset Down

         3: Preset Up +10 and round down to *0. if current preset is X9, it effect as +20 and round down. (no point double clicking if you are at X9)
            e.g., 22 -> 30; 29-> 40

         4: Preset Down +10 and round down to *0. if current preset is X8/X9, it round down to X1 instead of X0-10.
            e.g., 22 -> 10; 28 -> 21



As it now uses BLE it's possible to connect both mobile app and this pedal to Spark Amp. To do this you should first connect the pedal, and then launch the app, skipping the connect option. After that manually connect "Spark 40 Audio" to your mobile/tablet. Done, you can use audio/video options of the app, and all you have in the pedal. Note, that Tonecloud and preset editing from the app ain't available in this mode.

What it can for the time being:
*   IN ALL MODES
    *   The first rotary encoder is changing FX parameters like amp's knobs do (plus drive). Pushing encoder selects next FX. During this operation the second encoder changes to prev/next FX knobs
    *   Pushing the second encoder cycle modes
    *   Long pushing the second encoder involves deep-sleep (pushing wakes again)

*   IN "EFFECTS" mode: on-off individual effecs in a chain
    *   The second encoder changes presets: first four is hardware (in-amp), the rest are stored in flash. Pushing this encoder invokes SCENES mode )
    *   The four buttons switches FX on and off (DRV, MOD, DLY, RVB) or at your choice
    *   Long pressing the first button invokes amp info screen
    *   Long pressing the second button: bypassing all the 7 effects, and then any button returns the amp to the previous state

*   IN "SCENES" mode: switching between four configured presets as fast as Spark can. Switch between such configurations.
    *   First scene in the list is HW. This is an equivalent to amp's 4 channel buttons
    *   Each other scene contains 4 presets on user's choice
    *   Long pressing one of the four buttons saves current settings as a preset to the appropriate slot of the current scene 

*   "SETTINGS" mode: some parameters stored in flash
    *   Not implemented yet

It's gonna be a standalone BLE footswitch for Spark amp to use in "gig mode", ie to switch between tones (4 stored within the amp, and a reasonable number, say 20, 50 or 100 of presets stored in on-board flash), or just turn On/Off effects inside a tone with just one tap.

These presets will probably be accessible via the on-board web server thru WiFi connection (AP/Infrastructure mode). Or in some other way.

## ToDo

*   Storing presets in ESP's filesystem (DONE)
*   "SCENES" mode. Switching between 4-preset combinations (In progress).
*   Implement "ORGANIZE" mode (not gonna be here)
*   Implement "SETTINGS" mode
*   Accessing presets thru web-based UI via WiFi.
*   Storing master volume for every preset slot in flash.
*   There's a raw idea of auto-normalizing presets' master volumes.
*   Attaching addressable RGB LEDs.
*   Change to BLE (done, debugging).
*   Battery power supply and maybe battery management (In progress, probably will only work with Heltec).

## Your tone presets collection
*   There's a /data folder in the project, and for now you can populate it with subfolders like 1, 2, 3, 4, 5, etc, each containing a json file from the backup of your tones from the original PG app.
*   Don't forget to bild and upload a filesystem image as explained further!

## Software HowTo

[PlatformIO](https://platformio.org) is the recommended IDE for build and upload. It is a free add-on for MS VSCode which makes coding much simplier and satisfying.

1. Follow the instructions to install [VSCode and PlatformIO](https://platformio.org/install/ide?install=vscode)
2. Install Git
    - On Windows, install git from https://git-scm.com/download/win
    - On macOS, install Command Line Tools for Xcode running `xcode-select --install` via Terminal. Remember to run the command every time you update your macOS.
3. Launch VSCode and run the following commands (you can type there or select commands from the list):
    - from View->Command Palette... (Ctrl+Shift+P)
        - Git: Clone
        - You will be asked for the URL of the remote repository (<https://github.com/copych/BLE_Spark_pedal>) and the directory to place the local repository.
    - At the top left find PlatformIO->Project Tasks and select your environment (i.e. esp32doit-devkit-v1)
        - Click "Build" under General
        - Click "Upload" under General
        - Click “Upload File System Image” under Platform. (!!! CRITICAL !!! DON'T SKIP THIS STEP !!!)

## Hardware HowTo

Wiring:

![](/images/BLE_pedal_bb.png)

![](/images/2021-05-09%2018-23-49.JPG)
![](/images/2021-05-09%2018-24-17.JPG)

Hardware in the photos:

*   DOIT ESP32 DevKit v1
*   4 buttons
*   2 rotary encoders with buttons
*   SSD1306 duo-color OLED display (just top 16 lines of pixels are always yelow, and the rest are always blue, you can't manage individual pixel color)


## Credits/Thanks

*   Positive Grid for their Spark 40 Amp. "Positive Grid", "Spark" and other trade marks belong to their respected owners.
*   Paul Hamshere https://github.com/paulhamsh/ for his system approach and a great reverse engineering job. This project is based on forked SparkIO, SparkComms and some of his other classes.
*   Christopher Cook https://github.com/soundshed for the great desktop app for playing with the amp, and for some code look-up.
*   Kevin McGladdery for the pedal example
*   FB spark programming group for help and support
