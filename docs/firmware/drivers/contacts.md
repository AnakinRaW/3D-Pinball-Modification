# Contact driver

The driver watches the contacts on the pins E-1 and E-2 of the [port expander](../io/port-expander.md) and publishes every contact that closes. The playfield's hit targets are such contacts.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::ContactClosed` |
| Source | The pin, `0` for E-1 and `1` for E-2 |
| Timestamp | The moment INT fell, which the port expander reports with the change |
| Payload | None |

The driver publishes a closure only when the contact had been open for at least 10 ms before it, so a bouncing contact gives one event. A full queue drops the event.

`closed()` gives the pins whose contact is closed, as the port expander's last read found them.

## Device faults

The contacts count as [failed](../error-handling.md#device-faults) once no read of the port expander has worked for 1 s. Error cases are an expander that does not answer, a line held low or a lost bus. The error code `failed()` returns is the controller's error flags of the last failed read.

## Driver

```cpp
class ContactDriver : public Driver {
public:
    // attaches the queue and the pins E-1 and E-2 to the port expander and registers with the device
    // monitor; false when the port expander refuses the pins or the device monitor is full
    bool begin() override;

    // gets the pins whose contact is closed, bit 0 for E-1 and bit 1 for E-2
    uint8_t closed() const;

    // gets which parts have failed, bit 0 once no read of the port expander has worked for 1 s,
    // with the controller's error flags of the last failed read as the error code
    Fault failed() const override;
};
```
