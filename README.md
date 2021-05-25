# Bluetooth Low Energy (BLE) pedal for the PG Spark Amp

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/ea220b14059e479ab6a0419a1c4935f8)](https://www.codacy.com/gh/copych/BT_Spark_pedal/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=copych/BT_Spark_pedal&amp;utm_campaign=Badge_Grade)
[![](https://www.travis-ci.com/copych/BT_Spark_pedal.svg?branch=master)](https://www.travis-ci.com/github/copych/BT_Spark_pedal)

As it uses BLE (ie connects to " Spark 40 BLE" device), at the same time the app can stream audio to "Spark 40 Audio" device. 

What it can for the time being:

*   "EFFECTS" mode: on-off individual effecs in a chain
    *   The first rotary encoder is changing FX parameters like amp's knobs do. A push selects next FX
    *   The second encoder changes presets: first four is hardware (in-amp), the other 24 are hardcoded yet. Pushing it envokes not-implemented-yet PRESETS mode )
    *   The four buttons switches FX on and off (DRV, MOD, DLY, RVB) or at your choice
    *   Long pressing the first button invokes amp info screen
    *   Long pressing the second button: bypassing all the 7 effects, and then any button returns the amp to the previous state

*   "PRESETS" mode: switching between four configured presets as fast as Spark can. Switch between such configurations.
    *   In progress

*   "SETTINGS" mode: some parameters stored in flash
    *   Not implemented   

It's gonna be a standalone BLE footswitch for Spark amp to use in "gig mode", ie to switch between tones (4 stored within the amp, and a reasonable number, say 20, 50 or 100 of presets stored in on-board flash), or just turn On/Off effects inside a tone with just one tap.

These presets will probably be accessible via the on-board web server thru WiFi connection (AP/Infrastructure mode). Or in some other way.

## TODO

*   Storing presets in ESP's filesystem (partially done)
*   Implement "PRESETS" mode (merged with SCENES, in progress)
*   "SCENES" mode. Switching between 4-preset combinations (In progress).
*   Implement "ORGANIZE" mode (not gonna be here)
*   Implement "SETTINGS" mode
*   Accessing presets thru web-based UI via WiFi.
*   Storing master volume for every preset slot in flash.
*   There's a raw idea of auto-normalizing presets' master volumes.
*   Attaching addressable RGB LEDs.
*   Change to BLE (done, debugging).
*   Battery power supply and maybe battery management (In progress, probably will only work with Heltec).

## Hardware

Wiring coming soon

Hardware in the photos:

*   DOIT ESP32 DevKit v1
*   4 buttons
*   2 rotary encoders with buttons
*   SSD1306 duo-color OLED display (just top 16 lines of pixels are always yelow, and the rest are always blue, you can't manage individual pixel color)

![](/images/2021-05-09%2018-23-49.JPG)
![](/images/2021-05-09%2018-24-17.JPG)

## Credits/Thanks

*   Positive Grid for their Spark 40 Amp. "Positive Grid", "Spark" and other trade marks belong to their respected owners.
*   Paul Hamshere https://github.com/paulhamsh/ for his system approach and a great reverse engineering job. This project is based on forked SparkIO, SparkComms and some of his other classes.
*   Christopher Cook https://github.com/soundshed for the great desktop app for playing with the amp, and for some code look-up.
*   Kevin McGladdery for the pedal example
*   FB spark programming group for help and support
