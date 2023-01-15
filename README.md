# LoRaWAN

Small collection of my LoRaWAN related projects.

## pico-sx1262

Highly inspired by siuwahzhong's repository [lorawan-library-for-pico](https://github.com/siuwahzhong/lorawan-library-for-pico).
Borrowed DHT library from vmilea's repository [pico_dht](https://github.com/vmilea/pico_dht/).

Using a [Waveshare SX1262 Raspberry Pi Pico Board](https://www.waveshare.com/wiki/Pico-LoRa-SX1262-868M) and a common DHT22 sensors this software sends the temperature and humidity to TheThingsNetwork. 


### Installation

Connect a DHT22 Sensor as following, have a look at the [pinout guide](https://raspberrytips.com/raspberry-pi-pico-pinout/):

* 3.3V or 5V power: 3V3 (OUT) - PIN36
* DATA: GPIO14 - PIN19
* GND: GROUND - PIN3 (or any other GROUND pin)

Setup a device in TheThingsNetwork console and change EUIs and KEYs in `src/config.h`.

You will need a working Raspberry Pi Pico development environment and the pico-sdk to compile this.

    git clone --recurse-submodules https://github.com/Finn10111/LoRaWAN
    cd LoRaWAN/pico-sx1262
    export PICO_SDK_PATH=/usr/share/pico-sdk/ # if not set yet
    cmake -DPICO_BOARD=pico
    make

After compilation copy the pico_lorawan_dht22.uf2 to the Pico.

### Known issues

* I am not a C developer at all.
* Battery life is bad, really bad, like about a day. Don't know how sleep mode works.
* Code is messy and I don't check for any errors.
* Not ready for production use, trying to make it more stable and less energy hungry.
