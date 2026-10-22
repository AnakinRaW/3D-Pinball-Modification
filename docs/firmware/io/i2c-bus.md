# I²C bus

The I²C bus lets several drivers share one of the Teensy's I²C ports. Each of these drivers hands its transfers to the bus and is called back when a transfer has ended. None of them waits for the bus.

Two buses run in this build. `wireBus` runs `Wire` for the [rotary sensor's driver](../drivers/magnetic-rotary.md) and the [port expander](port-expander.md). `wire2Bus` runs `Wire2` for the [touch driver](../drivers/touch.md).

## Transfers

A transfer is one exchange with one device. A driver prepares each of its transfers once and attaches it in its `begin()`. After that, each `request()` asks the bus to run the transfer once. It returns at once and may be called from any interrupt. A driver may request a transfer that is still running. The bus then runs it once more after the current run, so the new run reads what changed in the meantime. Several requests during one run cause only one more run.

The bus takes the requested transfers in turn, in the order they were attached. A requested transfer therefore waits for at most one run of each other transfer.

The processor has one controller for each I²C port. The controller raises an interrupt whenever a transfer needs its next step. The bus runs in that interrupt. `request()` raises the same interrupt in software, so only the interrupt touches the controller. When a transfer has ended, the interrupt calls the transfer's done function with the controller's error flags, which are 0 when the transfer worked.

`requestAndWait()` attaches a transfer that is not attached yet, requests it and waits until it has ended. A driver uses it in `begin()` to set its device up before the main loop starts.

## Start-up

The bus sets itself up when a driver first calls `attach()` or `requestAndWait()`. The bus then lets the port's `Wire` object set up SDA and SCL, their pull-ups and a bus clock of 100 kHz. From then on the bus programs the controller directly and no longer calls the `Wire` object.

`attach()` and `requestAndWait()` work before the main loop only, where the main task calls the drivers' `begin()` one after another. The set-up at the first of them therefore needs no lock.

## Errors

A transfer ends early when the controller reports an error. The errors are a missing answer, lost arbitration, a FIFO error and a line held low past the timeout `Wire` configures. The bus then clears the controller and goes on with the next transfer. The driver that requested the failed transfer decides whether to request it again.

If a fault stops a device in the middle of a byte, the device can keep the data line low and block the bus. The bus then sends up to nine clock pulses, one for each bit of a byte and its acknowledge. That usually lets the device finish its byte and release the line. `Wire` frees the bus the same way.

## Existing libraries

The bus programs the I²C controller directly, because the I²C libraries for the Teensy 4.1 make their callers wait or poll. `Wire` waits in every call until the transfer has ended. [teensy4_i2c](https://github.com/Richard-Gemmell/teensy4_i2c) transfers without waiting, but it runs one transfer at a time and reports its end only when asked. Two drivers on one bus would have to ask it again and again whether their turn has come. [Drivers](../driver-design.md) in this build are called back by an interrupt instead.

## Interface

```cpp
namespace io {

// a transfer on an I²C bus: bytes written to one device, then bytes read from it
struct I2cTransfer {
    using Done = teensy::inplace_function<void(uint32_t errors), 16>;   // a lambda capturing this fits

    uint8_t        address;    // the device's address
    const uint8_t* out;        // the bytes written first, such as a register address
    uint8_t        outCount;   // the number of bytes written, 0 for a read alone
    uint8_t*       in;         // where the bytes read go
    uint8_t        inCount;    // the number of bytes read, 0 for a write alone
    Done           done;       // called in the bus's interrupt when the transfer has ended, with the
                               // controller's error flags, 0 when it worked
};

// an I²C port that runs the transfers of every driver on its bus, one after another, in its interrupt
class I2cBus {
public:
    // adds a transfer the bus runs on request, and sets the bus up at the first; before the main
    // loop only
    void attach(I2cTransfer& t);

    // asks the bus to run the transfer once and then call its done function; returns at once,
    // from any interrupt
    void request(I2cTransfer& t);

    // attaches the transfer if it is not yet, runs it and waits until it has ended; before the main
    // loop only. Gets the controller's error flags, 0 when it worked
    uint32_t requestAndWait(I2cTransfer& t);
};

extern I2cBus wireBus;    // the bus on Wire, run by LPI2C1, defined in the main file
extern I2cBus wire2Bus;   // the bus on Wire2, run by LPI2C4, defined in the main file

}
```
