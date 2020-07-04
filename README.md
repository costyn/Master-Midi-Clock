# Master Midi Clock

For my awesome new Behringer TD-3 I wanted a more precise way of setting the tempo, so I needed an external clock source.

I got the Sparkfun Midi Shield (https://www.sparkfun.com/products/12898) with an Arduino UNO R3.

## Features
* Start/stop button
* Trimpot for setting speed
* Tap tempo button for tapping the tempo
* 1602 I2C LCD screen to display current BPM and MIDI tick delay

## Hardware hookup
* The MIDI shield sits on top of the Arduino UNO
* The 1602 LCD display is attached to I2C interfaces (SDA/SCL) and 5v/GND.


## TODO/Wishlist
* Use for the remaining unused trimpot and button
* Build an enclosure out of lasercut acrylate
* Portable power: add a LiPo battery for portability
