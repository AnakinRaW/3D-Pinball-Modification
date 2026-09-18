# Messung am IR-Reflexsensor

## Die Messungen

| | Was offen ist |
|---|---|
| Messung B | Wie viel Signal eine Kugel auf dem Kandidaten für die neuen Platinen erzeugt |
| Messung C | Die Zeitkonstante τ desselben Kandidaten |

## Reihenfolge

- **B zuerst**, am Einbauplatz.
- **C nach B**, der Triggerpegel kommt aus den Kugelwerten von B.

## Wo gemessen wird

| Ort | Messungen |
|---|---|
| Einbauplatz des Sensors, Maschine stromlos, 3,3 V von der Bank | B |
| Sensor allein, Ort und Abstand ohne Belang | C |

## Material

| Anzahl | Teil | Für |
|---|---|---|
| 1 bis 3 | IR-Sensorplatine aus der Maschine, dreipolig | B, C |
| 1 | Kandidat für die neuen Platinen, wie eine dreipolige Sensorplatine verdrahtet | B |
| 1 | Widerstand 4,7 kΩ | B, C |
| 1 | Widerstand 100 Ω | B, C, an einer Originalplatine |
| 1 | Widerstand 220 Ω | B, C, am Kandidaten für die neuen Platinen |
| 1 | Spannungsquelle 3,3 V, Labornetzteil oder der 3V3-Pin eines Entwicklungsboards | B, C |
| 1 | Stahlkugel 9 mm aus der Maschine | B, C |
| 1 | Multimeter, Gleichspannung, Auflösung 1 mV | alle |
| 1 | Oszilloskop mit zwei Kanälen, Einzelschuss-Triggerung | nur C |
| | Steckbrett, Litze | alle |

**Die Sensorplatine wird nicht verändert.** Kein Nachlöten, keine Bauteile entfernen, Kontakt nur an den drei vorhandenen Anschlüssen.

## Anschlussbelegung der Sensorplatine

| Pin | Funktion |
|---|---|
| 1 | Versorgung. Speist die LED-Anode und den Kollektorwiderstand R1, 1,585 kΩ, der auf der Platine sitzt |
| 2 | Emitter des Fototransistors. Das ist der Messknoten |
| 3 | LED-Kathode. Wird geschaltet |

Die Platine hat keinen Massepin. Der LED-Strom kehrt über Pin 3 zurück, der Fotostrom über Pin 2.

**Pin 1 führt beide Zweige, Pin 3 allein den LED-Strom.** Wer den LED-Strom sucht, misst in Pin 3.

**Der Kandidat für die neuen Platinen wird genauso verdrahtet**, mit einem Unterschied: sein Kollektor geht direkt an Pin 1, ohne den 1,585 kΩ. So wird die neue Platine gebaut.

```
LED-Anode    ──►  Pin 1
Kollektor    ──►  Pin 1
Emitter      ──►  Pin 2
LED-Kathode  ──►  Pin 3
```

## Aufbau für B und C

**Der Sensor sitzt an seinem Einbauplatz in der Maschine**, im endgültigen Abstand über der Bahn, mit der echten Bahn dahinter. Die Maschine bleibt stromlos, ihr Stecker an Pin 1 bis 3 abgezogen, die Bank versorgt den Sensor allein.

GND heißt im Folgenden immer der **Minuspol des Netzteils**.

1. Netzteil auf **3,3 V** stellen und nachmessen, bevor der Sensor angeschlossen wird. Nicht 5 V.
2. Pluspol des Netzteils an **Pin 1**.
3. **4,7 kΩ** von **Pin 2** nach GND. Der Knoten an Pin 2 heißt ab hier **Messknoten**, dort messen Multimeter und Tastkopf.
4. **Emitterwiderstand** an **Pin 3**, sein anderes Bein bleibt als **loser Draht** frei. Welcher Wert, hängt an der Platine: **100 Ω** an einer Originalplatine, **220 Ω** am Kandidaten für die neuen Platinen. So läuft der jeweilige Kanal später auch.

```
3,3 V  ────────────────►  Pin 1

Pin 2  ──┬─────────────►  4,7 kΩ  ──►  GND
         │
         └──────────────  MESSKNOTEN

Pin 3  ──►  R_E  ──────►  loser Draht
            100 Ω        auf GND getippt:  LED leuchtet
            oder 220 Ω   abgehoben:        LED dunkel
```

5. **Funktionsprobe.** Den losen Draht auf GND legen, die LED leuchtet jetzt. Multimeter auf Gleichspannung, schwarze Messleitung an GND, rote an den Messknoten. Eine Hand vor den Sensor gehalten muss den Wert deutlich bewegen. Bewegt sich nichts, ist die Verdrahtung falsch.
6. **Strom mitmessen, bei jedem Sensor neu.** Der lose Draht bleibt auf GND. Multimeter mit je einer Messleitung an die beiden Beine des Emitterwiderstands, Ablesung notieren. V_F ist eine Eigenschaft der LED dieses Exemplars, also stellt sich bei jedem Sensor ein etwas anderer Strom ein. Den Widerstand später ausgebaut nachmessen.

## Messung B: Was eine Kugel zurückgibt

Multimeter am Messknoten gegen GND, Gleichspannung. Beleuchtung wie im Betrieb.

**Die Kugel liegt in der Bahn, an der Stelle über dem Sensor**, nicht an dessen Stirnfläche.

Vier Werte in einem Zug, ohne die Beleuchtung anzurühren. Danach `Kugel, LED an` und `freie Bahn, LED an` einmal wiederholen: weichen sie um mehr als ein paar Millivolt ab, zählt die Aufnahme nicht.

| | freie Bahn | Kugel |
|---|---|---|
| Draht auf GND, LED an | | |
| Draht abgehoben, LED aus | | |

**Erwartet:** rund 215 mV bei `Kugel, LED an` und rund 47 mV bei `freie Bahn, LED an`, das liefert ein Originalkanal an 100 Ω. Die beiden Dunkelwerte liegen bei wenigen Millivolt. Nähert sich eine Ablesung 2,47 V, ist der Kanal am Anschlag.

## Messung C: Die Zeitkonstante τ

Multimeter abnehmen, Tastkopf an denselben Messknoten, Masseklemme an GND.

```
Kopplung    DC
Zeitbasis   100 µs/div
Trigger     Einzelschuss, fallende Flanke
Pegel       auf halber Höhe zwischen den beiden Werten der Kugelspalte aus Messung B
```

**Tastkopf vorher am Kalibriersignal des Oszilloskops kompensieren.**

**Die Kugel liegt dabei in der Bahn über dem Sensor.**

### Die fallende Flanke

Draht fest auf GND halten, dann abheben. Nur diese Richtung: ab etwa 2 kΩ zeichnet Figure 6 t_r und t_f als eine Kurve, das Blatt sagt also keinen Unterschied zwischen den Flanken voraus. Prellt der Draht, gilt die letzte Flanke.

### Ablesen

Zwei gleichwertige Wege, einer davon genügt:

| Verfahren | Ergebnis |
|---|---|
| Cursor auf 63 % der Sprunghöhe | Die Zeit bis dorthin ist τ |
| Anstiegszeit von 10 auf 90 % messen | τ = t / 2,2 |

**Erwartet:** rund 53 µs an 4,7 kΩ, so viel gibt eine Originalplatine. Bis 70 µs steht die Auslegung.

## Wie viele Sensoren

**Messung B mit dem Kandidaten für die neuen Platinen.** Die drei Originalplatinen tragen den GP2S700HCP, die neuen ein anderes Bauteil mit anderem Fotostrom und anderem Sichtfeld. Das Ergebnis der Originalplatinen gilt für ihn nicht. Gleicher Aufbau, gleicher Einbauort, dieselben vier Werte.

**Messung C an einem Sensor.** τ hängt am Bauteiltyp und am Pull-down, nicht an der Montage.

## Ergebnisbogen

```
Sensor-Kennung                       ________
Versorgungsspannung, gemessen        ________ V
Pull-down, gemessen                  ________ kΩ
Emitterwiderstand, gemessen          ________ Ω
Abstand Sensor zur Bahn              ________ mm

MESSUNG B
Spannung über dem 220 Ω, LED an      ________ mV
daraus I_LED                         ________ mA
daraus V_F                           ________ V
freie Bahn, LED an                   ________ mV
freie Bahn, LED aus                  ________ mV
Kugel, LED an                        ________ mV
Kugel, LED aus                       ________ mV
Wiederholung: freie Bahn, LED an     ________ mV
Wiederholung: Kugel, LED an          ________ mV
daraus Signal, beide Differenzen     ________ mV
daraus Dunkelverschiebung            ________ mV

MESSUNG C
τ, fallende Flanke                   ________ µs
```
