# Zephyr on the Teensy 4.1

Zephyr was measured on the Teensy 4.1 against the FreeRTOS stack test and was rejected. The build used Zephyr 4.5.99 from the main branch, LVGL 9.6.0, optimization for speed, and the same sequence as the FreeRTOS stack test: an SD card file of 60 frames of 320 × 240 RGB565, read whole into the PSRAM by DMA, and LVGL drawing that frame full-screen into two buffers in RAM2.

| Measurement | Zephyr | FreeRTOS stack test |
|---|---|---|
| PSRAM through Zephyr's `nxp,imx-flexspi-aps6404l` driver | chip ID read, every AHB read returns zero | works |
| PSRAM through Teensyduino's `configure_external_ram()` carried over | 8 MB without error, 31 MB/s write, 32 MB/s read | works |
| Processor left to the main thread while the storage thread reads | 90 to 93 % | 98.7 % |
| One frame read whole | 12.2 ms mean | 16.56 ms mean, 18.01 ms longest |
| SD reads | aborted with `Card read failed -5` after 85 to 160 frames in three of four runs | stable |
| LVGL draws the PSRAM frame, its code in the flash | 6.1 ms, 9.4 ms while the card reads | 5.33 ms, 6.86 ms while the card reads |

## Board support

The `teensy41` board of Zephyr declares the SD socket (`usdhc1` with `zephyr,sdmmc-disk`) and the program flash. It declares no PSRAM, no FlexSPI2 node, no I2S and no FlexIO. Flashing goes through the PJRC bootloader with `west flash`, and the USB console comes from the `cdc-acm-console` snippet. A Zephyr program does not answer the Teensy Loader's reboot request, so the program button is needed unless the program watches for a host setting 134 baud and executes `bkpt #251`.

## Drivers

| Need | In Zephyr |
|---|---|
| 8080 display bus on FlexIO | `mipi_dbi_nxp_flexio_lcdif`, untested on the Teensy 4.1 |
| WS2812 on six chains in parallel | none. The `led_strip` drivers cover GPIO, I2S, SPI, UART, Pulse-IO, Pico PIO and BFLB, one chain each |
| Four WAV streams mixed to I2S | the I2S interface only, no player and no mixer |
| LVGL | 9.6.0 as a module, its code in the flash without a linker script of the project |
| SD card | uSDHC with ADMA2, the thread sleeps on a semaphore during a transfer |

Sources: the board's [documentation](https://docs.zephyrproject.org/latest/boards/pjrc/teensy4/doc/index.html) and [devicetree](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/pjrc/teensy4/teensy41.dts), Zephyr's [`drivers/mipi_dbi`](https://github.com/zephyrproject-rtos/zephyr/tree/main/drivers/mipi_dbi), [`drivers/led_strip`](https://github.com/zephyrproject-rtos/zephyr/tree/main/drivers/led_strip) and [`west.yml`](https://github.com/zephyrproject-rtos/zephyr/blob/main/west.yml).

## Not settled

The cause of the SD aborts and the cause of the zero reads through Zephyr's PSRAM driver were not looked for.
