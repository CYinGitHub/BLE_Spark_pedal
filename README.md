# Yet another bluetooth pedal for the PG Spark Amp
It now can handle some basic functions, but it's extremely buggy and cut yet.
It's gonna be a standalone bluetooth footswitch for Spark amp to use in "gig mode", ie to switch between tones (4 stored within the amp, and a reasonable number, say 20, 50 or 100 of presets stored in on-board flash), or just turn On/Off effects inside a tone with just one tap.

These presets will probably be accessible via the on-board web server thru WiFi connection (AP/Infrastructure mode). Or in some other way.

## Hardware
It's ESP32 based.
Pedal's hardware may include some momentary buttons, and optionally rotary encoders.

## Wiring
Coming soon

Hardware in the photos:
* DOIT ESP32 DevKit v1
* 4 buttons
* 2 rotary encoders with buttons
* SSD1306 duo-color OLED display (just top 16 lines of pixels are always yelow, and the rest are always blue, you can't manage individual pixel color)

![](/images/2021-05-09%2018-23-49.JPG)
![](/images/2021-05-09%2018-24-17.JPG)

## Credits/Thanks
* PositiveGrid for their Spark 40 Amp.
* Paul Hamshere https://github.com/paulhamsh/ for his system approach and a great reverse engineering job. This project is based on forked SparkIO, SparkComms and some of his other classes.
* Christopher Cook https://github.com/soundshed for the great desktop app for playing with the amp, and for some code look-up.
