# Resources

The files in this folder go onto the root of the machine's memory card, each with its path below this folder. `config.yml` becomes `/config.yml`, the [machine config](../../../docs/firmware/components/config.md).

`python -m platformio run -t uploadcard` puts them onto the card in the Teensy over USB, through the program in [`firmware/tools/card-upload/`](../../tools/card-upload/). This README stays behind.
