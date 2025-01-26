# README #

### What is this repository for? ###

* A template STM32CubeMX project to make rapid SW prototyping easier
* this allows one to use CubeMX to auto generate code to setup the peripherals
* the following are already setup:
    * clocks configured for maximum core clock
    * embedded-cli setup over LPUART, to talk to your PC via the serial to USB converter built into the ST-link V3 on the nucleo board
    * fault manager
    * reset handler

### How do I get set up? ###

* Clone this repo to your local directory
* open the directory with VScode
* install the STM32 extension for VScode (requires CubeMX, etc to be installed)
* compile using the 'Build' button in the bottom toolbar
* flash using Ozone and a J-link