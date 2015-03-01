# Interrupt-based evdev input driver for Raspberry Pi GPIO

This is a Linux kernel module for the Raspberry Pi single-board computer that exposes edge-detection events on a GPIO input pin via the generic input event interface (evdev).

## Building

These steps assume you're cross-compiling the module and that your `PATH` contains a toolchain prefixed by `armv6j-hardfloat-linux-gnueabi-`. It is also possible to build the module on the Pi itself, but you'll need to tweak the Makefile.

1. Clone the Raspberry Pi Linux kernel sources for the version of Linux you're running on your Pi:

        $ mkdir raspi && cd raspi
        $ git clone -b rpi-3.18.y https://github.com/raspberrypi/linux.git

1. Copy /proc/config.gz from your Pi and decompress it to linux/.config:

        $ zcat config.gz > linux/.config

1. Clone this repository:

        $ git clone https://github.com/whitslack/rpi-gpio_input.git gpio_input
        $ cd gpio_input

1. Build:

        $ make

1. Copy the resulting `gpio_input.ko` to your Pi.

## Using

1. Insert the module into your running kernel on the Pi:

        $ insmod gpio_input.ko

   The module prints a line to the kernel log when it's loaded:

        input: GPIO Input as /devices/platform/gpio_input/input/input0

1. You'll find a new device node at `/dev/input/by-path/platform-gpio_input-event`. You can read input events directly from this device node. The included `dump_events` utility will dump an ASCII listing of edge detection events, or you can write your own program to do something useful with them.

        $ ./dump_events < /dev/input/by-path/platform-gpio_input-event
