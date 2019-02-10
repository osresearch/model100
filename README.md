TRS-80 Model iCE100
====

This replaces the mainboard of the TRS-80 Model 100 with an iCE40 FPGA
that can act as a serial or graphics console to a USB attached device.

More info: https://trmm.net/TRS80_Model_100

Pinout
===

The LCD is 240x64 and uses ten Hitachi HD44102 LCD controllers,
each with its own select line and shared control lines.

The LCD requires a charge pump to generate the -5V required for the bias
voltage.  This is generated with a duty cycle controlled PWM output.

LCD connector HU-30P-2G-L13

Pin | Function
----|---------
  1 | VDD
  2 | BZ - buzzer on the front panel (optional)
  3 | VEE -- negative voltage (-5V)
  4 | V2 - contrast voltage (0 - 5V)
  5 | GND
  6 | GND
  7 | CS24
  8 | CS23
  9 | CS29
 10 | CS22
 11 | CS28
 12 | CS21
 13 | CS27
 14 | CS20
 15 | CS26
 16 | CS25
 17 | RESET (can be pulled high for always reset)
 18 | CS1
 19 | E
 20 | R/!W (can be pulled low for always write)
 21 | D/!I
 22 | AD0
 23 | AD1
 24 | AD2
 25 | AD3
 26 | AD4
 27 | AD5
 28 | AD6
 29 | AD7
 30 | NC


LCD bias voltage
===

The LCD requires negative voltage, which is generated with a two diode,
two capacitor charge pump.

           +
    PWM ---||---+--|<--+-- OUT
                |      |
                \/    ---
                --    ---+
                |      |
    GND --------+------+


If it is driven with a 3V signal, the minimum output is around -2 V.
Driving with a 5V PWM produces  -3 V, which gives better contrast.

            5V
             |
             # 100 Ohm
             |
             +-------------- 5V PWM out
             |
             /
    PWM ----|
             \
             v
             |
            GND        

Image format
===
    convert -resize !240x64 -depth 1 -monochrome -rotate 90 \
	logo.png logo.gray
    xxd -g8 -c8 -p logo.gray > fb.hex


Keyboard
====

The 8x9 keyboard matrix has diodes on every key so it is possible to
identify any number of pressed keys.  However, due to the direction of
the diodes this requires that a slightly odd mechanism be used.

The 8 rows are all configured as input pullups and the 8 of the 9 columns
are driven high.  All the rows are read and any that have been pulled low
due to the diode sinking current to ground are pressed.

          3V
           |
           # internal pullup
           |
    Row ---+---->|--/ --- Col


Upduino v2
===

J6 1.7 5 90
JP6 39.5 1.8 180
JP5 39.5 19.58 180

