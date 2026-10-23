# Connectors

## By subsystem

| Subsystem | Connection | Poles | Made with | Reason |
|---|---|---|---|---|
| Power board | J-IN | 2 | screw terminal | The adapter's full current on 16 AWG, which fits no Dupont contact. Without Q1, a swapped pair destroys the Teensy and the ICs |
| | J-LIGHT1, J-LIGHT2 | 2 each | soldered at both ends | The [lighting design](parts/lighting/design.md#supply) requires it |
| | J-SOL | 2 | screw terminal | All four coils at once, more than a derated Dupont contact carries |
| | J-SERVO, J-MAIN, J-IR, J-DISP | 2 each | screw terminal | 5 V. A reversed Dupont plug would reverse the supply |
| Teensy motherboard | the Teensy | 2 × 24 | socket headers | The Teensy stays pluggable |
| | J-MB-PWR, from J-MAIN | 2 | screw terminal | 5 V |
| | the signal to the servo board | 1 | soldered | It comes apart at the servo board's J-SERVO-PWR |
| IR sensing | input of the D24V5F3 | 2 | soldered | The module has no reverse-voltage protection. VIN, GND and VOUT sit in one row |
| | the D24V5F3 on the IR mainboard | 4 | socket header | The module stays pluggable. On the bench the Teensy's 3V3 feeds the socket in its place |
| | J-IR-TEENSY, both ends | 8 | Dupont | The board's lines to the Teensy |
| | J-S1 to J-S16 | 3 each | Dupont | 3.3 V, with no Teensy pin behind it. The stock sensors' cables end in Dupont plugs. A new sensor board's cable is soldered at the sensor |
| Solenoids | J-COIL-PWR of the coil board | 2 | screw terminal | The coils' current |
| | J-COIL-TEENSY of the coil board, both ends | 4 | Dupont | The four triggers |
| | J-C1 to J-C4 | 2 each | Dupont | A coil has no polarity. The stock coils' cables end in Dupont plugs |
| | J-SENSE-TEENSY of the sense board, both ends | 5 | Dupont | The three senses, the Teensy's 3V3 and GND |
| | J-M | 4 | Dupont at the board, eyelets at the foil and the shells | 3.3 V. The series resistors sit between J-M and the Teensy |
| Lighting, each of the two boards | J-AMBIENT-PWR, J-PLAYFIELD-PWR | 2 | soldered | The other end of J-LIGHT1 or J-LIGHT2 |
| | data from the Teensy, both ends | 4 | Dupont | The board's three chains and GND at 3.3 V |
| | J-LA to J-LC on the ambient board, J-LD to J-LF on the playfield board | 3 each | screw terminal | 5 V with a whole chain's current. A reversed plug would reverse the LEDs' supply |
| | segment to segment | 3 each | soldered | |
| Audio | the DFR0954 on the Teensy motherboard | 2 × 6 | socket headers | DFRobot includes no header |
| | speaker | 2 | PH2.0 at the module, the stock plug at the speaker | Polarity does not matter |
| Servo | J-SERVO-PWR of the servo board | 3 | screw terminal | 5 V beside a Teensy pin |
| | servo plug | 3 | pin header | The servo's cable stays whole |
| Magnetic rotary sensor | Teensy and module | 4 | Dupont | 3.3 V and I²C |
| Display | J-DISP-PWR of the display board | 2 | Dupont | |
| | touch cable, both ends | 3 | Dupont | SDA, SCL and 3V3 of the Teensy |
| | display board to the module | 4 | soldered at both ends | The cable carries 5 V. Vin sits on the module's header beside the bus |
| | the Adafruit 757 on the display board | 2 × 6 | socket headers | The 757 comes with two 6-pin headers |
| | bus | 12 | Dupont at the Teensy, soldered at the module | Vin sits on the module's header beside the bus |
| Break beam | at the emitter and the receiver | 2 each | soldered | The bare parts sit in the niches |
| | at the Teensy motherboard | 2 each | Dupont | One plug for the emitter, with the feed behind R2 and GND. One for the receiver, with its output and GND |
| Controls | toggle switch | 2 | the stock plug's counterpart at the switch, Dupont at the Teensy | A switch has no polarity |

## By pole count

| Poles | Screw terminal | Dupont or socket header | Counterpart to a stock or module plug |
|---|---|---|---|
| 2 | 8 (six on the power board, J-MB-PWR and J-COIL-PWR) | 8 (the toggle switch, J-C1 to J-C4, the break beam's emitter and receiver, J-DISP-PWR) | the toggle switch, the speaker, PH2.0 at the DFR0954 |
| 3 | 7 (the six lighting chains, J-SERVO-PWR) | up to 16 J-S, 2 touch, the servo plug | |
| 4 | | 10 (4 lighting data, 2 rotary sensor, 2 J-COIL-TEENSY, J-M, the D24V5F3's socket) | |
| 5 | | 2 (J-SENSE-TEENSY) | |
| 6 | | 4 socket headers, two for the Adafruit 757 and two for the DFR0954 | |
| 8 | | 2 (J-IR-TEENSY) | |
| 12 | | 1 (the display bus) | |
| 24 | | 2 socket headers for the Teensy | |

## Wire colors

| Color | Carries | Ends at the Teensy on |
|---|---|---|
| Black | GND | GND |
| Red | 5 V, also behind a diode or a resistor, as VIN behind D1, the emitter behind R2 and GAIN behind R4 | VIN only |
| Yellow | 3.3 V supply, also behind a resistor, as the Teensy's 3V3, the IR mainboard's own rail and the foil behind R91 | 3V3 |
| Blue | 3.3 V logic going out from the Teensy, also behind a series resistor or the isolator | an output |
| Green | 3.3 V signals coming in, such as a sense, a contact, the receiver, an IR channel, MISO or INT | an input. Each one reads 0 V against GND with the supply on before the Teensy is first plugged in |
| White | I²C at 3.3 V, and every line behind a level shifter or a driver, as the lighting data, the servo pulse, the display module's I²C, the coil drains, the LED bus and the speaker | the pins of `Wire` and `Wire2` only |

| Where | Colors |
|---|---|
| A cable that comes with a part or from the stock machine, as the servo's, the speaker's and the coils' | its own. Black there can carry a signal |
| J-S1 to J-S16, the stock sensors' cables and a new sensor board's alike | red 3.3 V, black signal, yellow LED cathode |
| Display board | white on both sides of the Adafruit 757. The touch cable brings a yellow 3.3 V, the cable to the module a red 5 V |
