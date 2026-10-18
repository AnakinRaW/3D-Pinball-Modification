# Connectors

## By subsystem

| Subsystem | Connection | Poles | Made with | Reason |
|---|---|---|---|---|
| Power board | J-IN | 2 | screw terminal | The adapter's full current on 16 AWG, which fits no Dupont contact. Without Q1, a swapped pair destroys the Teensy and the ICs |
| | J-LIGHT | 2 | soldered at both ends | The [lighting design](parts/lighting/design.md#supply) requires it |
| | J-SOL | 2 | screw terminal | All four coils at once, more than a derated Dupont contact carries |
| | J-SERVO, J-AMP, J-TEENSY, J-IR, J-DISP | 2 each | screw terminal | 5 V. A reversed Dupont plug would reverse the supply |
| Teensy carrier board | the Teensy | 2 × 24 | socket headers | The Teensy stays pluggable |
| | VIN and GND from J-TEENSY | 2 | screw terminal | 5 V |
| | the signal to the servo board | 1 | soldered | It comes apart at the servo board's J-PWR |
| IR sensing | input of the D24V5F3 | 2 | soldered | The module has no reverse-voltage protection. VIN, GND and VOUT sit in one row |
| | J-PWR | 2 | screw terminal | Reverse-proof, as the [IR design](parts/ir-reflective/design.md#connections) requires. On the bench it is clamped onto the Teensy's 3V3 |
| | J-T, both ends | 8 | Dupont, coded | Reverse-proof, as the IR design requires |
| | J-S1 to J-S16 | 3 each | Dupont | 3.3 V, with no Teensy pin behind it. The stock sensors' cables end in Dupont plugs. A new sensor board's cable is soldered at the sensor |
| Solenoids | J-PWR of the coil board | 2 | screw terminal | The coils' current. Reverse-proof, as the [solenoid design](parts/solenoid/design.md#connectors) requires |
| | J-T of the coil board, both ends | 4 | Dupont, coded | Reverse-proof, as the solenoid design requires |
| | J-C1 to J-C4 | 2 each | Dupont | A coil has no polarity. The stock coils' cables end in Dupont plugs |
| | J-T of the sense board, both ends | 5 | Dupont, coded | Reverse-proof, as the solenoid design requires |
| | J-M | 4 | Dupont at the board, eyelets at the foil and the shells | 3.3 V. The series resistors sit between J-M and the Teensy |
| Lighting | J-PWR | 2 | soldered | The other end of J-LIGHT |
| | data from the Teensy, both ends | 7 | Dupont, coded | Six chains and GND at 3.3 V |
| | J-LA to J-LF | 3 each | screw terminal | 5 V with a whole chain's current. A reversed plug would reverse the LEDs' supply |
| | segment to segment | 3 each | soldered | |
| Audio | J-PWR of the amplifier board | 2 | screw terminal | 5 V |
| | I²S cable, both ends | 4 | Dupont, coded | DIN, LRC, BCLK and GND of the Teensy |
| | the DFR0954 on the amplifier board | its pin row | socket header | DFRobot includes no header |
| | speaker | 2 | PH2.0 at the module, the stock plug at the speaker | Polarity does not matter |
| Servo | J-PWR of the servo board | 3 | screw terminal | 5 V beside a Teensy pin |
| | servo plug | 3 | pin header | The servo's cable stays whole |
| Magnetic rotary sensor | Teensy and module | 4 | Dupont, coded | 3.3 V and I²C |
| Display | J-PWR of the display board | 2 | screw terminal | 5 V |
| | touch cable, both ends | 3 | Dupont, coded | SDA, SCL and 3V3 of the Teensy |
| | display board to the module | 4 | soldered at both ends | The cable carries 5 V. Vin sits on the module's header beside the bus |
| | the Adafruit 757 on the display board | 2 × 6 | socket headers | The 757 comes with two 6-pin headers |
| | bus | 12 | Dupont, coded at the Teensy, soldered at the module | Vin sits on the module's header beside the bus |
| Break beam | at the sensor | 3 | soldered | Emitter and receiver share 3V3 and GND |
| | at the Teensy | 3 | Dupont, coded | 3V3, GND and the receiver's output |
| Controls | toggle switch | 2 | the stock plug's counterpart at the switch, Dupont at the Teensy | A switch has no polarity |

## By pole count

| Poles | Screw terminal | Dupont or socket header | Counterpart to a stock or module plug |
|---|---|---|---|
| 2 | 12 (seven on the power board, VIN on the Teensy carrier board, J-PWR on the IR mainboard, the coil board, the display board and the amplifier board) | 5 (the toggle switch, J-C1 to J-C4) | the toggle switch, the speaker, PH2.0 at the DFR0954 |
| 3 | 7 (the six lighting chains, J-PWR of the servo board) | up to 16 J-S, 2 touch, the break beam, the servo plug | |
| 4 | | 7 (2 audio, 2 rotary sensor, 2 J-T of the coil board, J-M) | |
| 5 | | 2 (J-T of the sense board) | |
| 6 | | 2 socket headers for the Adafruit 757 | |
| 7 | | 2 (lighting data) | |
| 8 | | 2 (IR J-T) | |
| 12 | | 1 (the display bus) | |
| 24 | | 2 socket headers for the Teensy | |
