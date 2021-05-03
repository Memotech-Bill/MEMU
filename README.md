# MEMU - Memotech EMUlator

The [Memotech MTX](https://en.wikipedia.org/wiki/Memotech_MTX) series of computers were superior
8-bit computers of the 1980s. Although initially supplied with a ROM BASIC later addons included
disk drives of various formats, which could be accessed from extensions to the ROM BASIC or the
system could boot CP/M 2.2.

MEMU is a full featured emulator of the MTX systems originally written by
Andy Key. Andy's version of MEMU is available for Microsoft Windows or Linux from
[here](http://www.nyangau.org/memu/memu.htm).

This code is a port of MEMU to run on a Raspberry Pi Pico. Due to the capacity of the Pico
this is a cut-down version with only some of the capabilities of the original MEMU. Amongst
other limitations, the MTX machines could support more RAM (by paging) than the Pico has.

This version of the code is very much a work in progress. It is only capable of building
the Pico executable, although the source contains elements of the code for other builds.
It is intended to eventually re-merge this code with that needed to provide a single source
that can build versions for all targets. There are probably bugs, and there is certainly
diagnostic code.

The definitive source of information about Memotech computers is
[Dave Stevenson's site](http://primrosebank.net/computers/mtx/mtx512.htm).

## Hardware Requirements

* Raspberry Pi Pico with headers
* Pimoroni Pico VGA Demo Base
* VGA Monitor
* VGA Cable
* USB keyboard - Not all keyboards work with tinyusb. A cheap one may be best
* USB to Micro-USB adaptor to connect the keybord to the Pico
* 5V Power Supply with Micro-USB connector
* Micro-SD Card
* Optional: Headphones or Amplified Speakers

## Building the Pico Version

It is recommended that the build be performed on a Raspberry Pi. This is the only build
that has been tested.

     mkdir pico
     cd pico
     wget https://raw.githubusercontent.com/raspberrypi/pico-setup/master/pico_setup.sh
     chmod +x pico_setup.sh
     ./pico_setup.sh
     git clone https://github.com/Memotech-Bill/memu.git
     cd memu
     mkdir build
     cd build
     cmake -DTARGET=Pico ..
     make

The resulting memu-pico.uf2 file can then be copied onto the Pico, or the memu-pico.elf
file loaded using the debug connector on the Pico. See
["Getting Started with Raspberry Pi Pico"](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf).

## Usage

The SD card should be FAT formatted.

* Copy the contents of the "sd_card" folder to the root of the SD card.
  * The "/disks" folder contains floppy disk images
  * The "/tapes" folder contains tape images
* These are only a few examples. Many more files can be obtained from Andy Key's
[original distribution](http://www.nyangau.org/memu/download.htm) and
[disks collection](http://www.nyangau.org/diskimages/diskimages.htm).
* Plug the Pico into the VGA Demo Base
* Connect the keyboard to the USB socket on the Pico
* Connect the Power Supply to the USB socket on the VGA Demo Base
* Connect the Monitor
* Insert the SD card into the socket on the VGA Demo Base
* If used, connect the headphones or speaker to the DAC socket on the VGA Demo Base
(not the PWM) socket

On initial power-up the Pico should start in emulated ROM BASIC.

* The &lt;SysReq> key opens a dialog from which to configure the emulation.
* Memotech CP/M systems have two seperate video outputs:
  * a 40-column display from the MTX
  * an additional 80-column display as part of the disk system
* On the Pico emulation, the key combinations &lt;Alt+F1> and &lt;Alt+F2> switch between these two displays.

## Licence

To conform with the terms of Andy Key's original release, all the MEMU specific code is
released according to the [Unlicense](https://unlicense.org/).

This port of MEMU relies on other open source software which is subject to other terms,
see appropriate source files:

* FatFS - Copyright (C) 20xx, ChaN
* Z80 Emulation - Copyright (C) Marat Fayzullin 1994,1995,1996,1997
