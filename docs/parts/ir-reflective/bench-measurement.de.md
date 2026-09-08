# Messung am IR-Reflexsensor

Arbeitsanleitung zur Aufnahme von fünf Zahlen, die in keinem Datenblatt stehen und an denen die Auslegung der Ballerkennung hängt. Netzspannung ist nirgends beteiligt.

## Die fünf Messungen

| | Was offen ist |
|---|---|
| Messung A | Welchen Strom die Stockmaschine durch die LED treibt und welche Flussspannung dabei anliegt. Bekannt sind 10 mA als Mittelwert über hell und dunkel |
| Messung B | Wie viel Signal eine Kugel erzeugt. Bisher eine Annahme von 50 µA |
| Messung C | Die Zeitkonstante τ des Sensors. Zwei Hochrechnungen stehen um Faktor 3,5 auseinander, 62 µs gegen 214 µs |
| Messung D | Wie viel Gleichanteil das Umgebungslicht verbraucht und wie stark es flackert. Bisher 258 mV und 60 % Modulationstiefe bei 100 Hz |
| Messung E | Die Konstante k, mit der die Firmware den Rest des Umgebungslichts herausrechnet |

## Reihenfolge

Jede Messung liefert eine Eingangsgröße der nächsten.

- **A zuerst.** Die Maschine muss dafür zusammengebaut sein und laufen, mit der Sensorplatine an ihrem Mainboard. B bis D nehmen eine Platine von diesem Stecker ab.
- **B danach**, am Einbauplatz. B setzt das Signal, gegen das alles andere bemessen wird.
- **C nach B**, weil der Triggerpegel aus den Kugelwerten von B kommt.
- **D nach B**, weil der Gleichanteil aus D dieselbe Ablesung ist wie "freie Bahn, LED aus" aus B, an demselben Knoten mit derselben Meterstellung.
- **E zuletzt.** Der Gleichanteil aus D setzt zweierlei: die Schwelle, ab der k überhaupt einen Wandlerschritt wert ist, und die Genauigkeit, mit der k gemessen werden muss.

## Wo gemessen wird

| Ort | Messungen |
|---|---|
| Stockmaschine, zusammengebaut und laufend | A |
| Einbauplatz des Sensors, Maschine stromlos, 3,3 V von der Bank | B, D |
| Sensor allein, Ort und Abstand ohne Belang | C |
| Regler allein am Steckbrett | E, erster Teil |
| Fertiges Board mit seinem endgültigen Kabel | E, zweiter Teil |

Die Kalibrierwerte je Kanal, `clear` und `ball`, nimmt der Treiber später auf dem fertigen Board mit allen sechzehn Kanälen auf. Das ist eine andere Messung.

## Material

| Anzahl | Teil | Für |
|---|---|---|
| 1 | Die zusammengebaute Stockmaschine, betriebsbereit | A |
| 1 bis 3 | IR-Sensorplatine aus der Maschine, dreipolig | A, B, C, D |
| 1 | Widerstand 10 Ω, 4 mW | A, Weg 1 |
| 1 | Widerstand 180 Ω, 68 mW dauerhaft | A, Weg 2 |
| 1 | Widerstand 4,7 kΩ | B, C, D |
| 1 | Widerstand 220 Ω | B, C |
| 1 | Spannungsquelle 3,3 V, Labornetzteil oder der 3V3-Pin eines Entwicklungsboards | B, C, D |
| 1 | Stahlkugel 9 mm aus der Maschine | B, C |
| 1 | Pololu D24V5F3 | E |
| 1 | Widerstand 180 Ω, 60 mW | E |
| 2 | Widerstand 36 Ω, **je 0,30 W** | E |
| 1 | Spannungsquelle 5 V | E |
| 1 | Multimeter, Gleichspannung, Auflösung 1 mV, mit Relativfunktion | alle |
| 1 | Oszilloskop mit Einzelschuss-Triggerung | A, C, D |
| | Steckbrett, Litze | alle |

Die beiden 36 Ω verheizen je 0,30 W. Ein 0,25-W-Bauteil hält das für die wenigen Sekunden der Ablesung aus, weil in k nicht der Widerstandswert steht, sondern nur der Strom, den er zieht: 5 % Drift kosten 9 mA von 165 mA Laststufe und damit unter 2 mV Reglerspannung.

**Die Sensorplatine wird nicht verändert.** Kein Nachlöten, keine Bauteile entfernen. Sie geht nach der Messung unverändert in die Maschine zurück, und der Kontakt erfolgt nur an den drei vorhandenen Anschlüssen.

## Anschlussbelegung der Sensorplatine

| Pin | Funktion |
|---|---|
| 1 | Versorgung. Speist die LED-Anode und den Kollektorwiderstand R1, 1,585 kΩ, der auf der Platine sitzt |
| 2 | Emitter des Fototransistors. Das ist der Messknoten |
| 3 | LED-Kathode. Wird geschaltet |

Die Platine hat keinen Massepin. Der LED-Strom kehrt über Pin 3 zurück, der Fotostrom über Pin 2.

**Pin 1 führt beide Zweige, Pin 3 allein den LED-Strom.** Der Fototransistorzweig erreicht an der Stockmaschine bis zu 2,65 mA, aus `(5 − 0,6 − 0,2) V / 1,585 kΩ`, weil das Mainboard Pin 2 auf eine Basis-Emitter-Strecke legt und dort bei 0,6 V klemmt. Wer den LED-Strom sucht, misst in Pin 3.

## Messung A: Was die Stockmaschine treibt

**Diese Messung läuft an der Stockmaschine.** Sie muss dafür zusammengebaut sein und laufen, mit der Sensorplatine an ihrem Mainboard.

> [!CAUTION]
> **Pin 3 niemals ohne Vorwiderstand auf GND.** Die Sensorplatine hat keine Strombegrenzung für die LED, und der 180 Ω der Stockmaschine sitzt auf dem Mainboard hinter Pin 3. Pin 3 direkt auf GND hängt die LED an 5 V und zerstört sie. Mit einem Widerstand davor ist es unbedenklich.

> [!CAUTION]
> **Maschine stromlos machen, bevor der Shunt in den Kabelbaum kommt.** Pin 1 ist die 5-V-Rail der Maschine, und ein Abrutschen auf Masse belastet ihr Netzteil mit einem Kurzschluss.

Ein Multimeter im Strombereich zeigt den Mittelwert. Der Emitter der Stockmaschine pulst mit 50 % Tastverhältnis bei 333 Hz, also stehen die gemessenen 10 mA an Pin 1 für den Mittelwert über hell und dunkel, nicht für den Strom in der hellen Halbwelle. Die Rekonstruktion sagt dort 18,3 mA.

**Die Flussspannung wird mitgenommen, entscheidet aber nichts.** An der Stockmaschine liegen Pin 1 auf 5 V und Pin 3 in der hellen Halbwelle auf 3,5 V, deren Differenz von 1,5 V über dem Tabellenmaximum des Datenblatts von 1,4 V liegt. Der 180 Ω und Q1 sitzen unter Pin 3 und gehen in diese Differenz nicht ein.

Beide Enden des Bereichs sind ausgerechnet, und die Auslegung hält an beiden. Ein niedriges V_F treibt den Strom hoch, und bei 1,18 V mit Rail-Maximum stehen 10,3 mA gegen den 50-mA-Grenzwert der LED. Ein hohes V_F kostet Licht, und der Unterschied zwischen 1,4 V und 1,5 V bewegt den kleinsten Emitterstrom von 7,9 mA auf 7,4 mA. Diese 6 % gehen in die 50 µA Fotostrom, die Messung B als Annahme ersetzt.

Es gibt zwei Wege, und sie messen nicht dasselbe. Das Mainboard wurde nie durchgemessen, sein Schaltplan ist aus Bauteilaufdrucken und Oszilloskopbildern rekonstruiert, und ein vierter Widerstand ist bis heute nicht abgelesen. Weg 1 misst, was die Maschine tut. Weg 2 misst, was die Rekonstruktion vorhersagt. Stimmen beide überein, ist die Rekonstruktion belegt.

### Weg 1: An der laufenden Maschine

Die Sensorplatine bleibt am Mainboard. **10 Ω in die Leitung zu Pin 3**, Tastkopf über den Widerstand, Masseklemme an die Masse der Maschine.

```
Pin 1  ──  Mainboard, unverändert          Tastkopf: Pin 1 gegen GND
Pin 2  ──  Mainboard, unverändert
Pin 3  ──►  10 Ω  ──►  Mainboard           Tastkopf: über den 10 Ω
```

Der Verlauf zeigt beide Halbwellen einzeln, und das obere Plateau geteilt durch den gemessenen Widerstandswert ist der LED-Strom ohne Umweg über das Tastverhältnis. 18 mA erzeugen 180 mV, das reicht auch bei grober Einstellung.

Zwei Dinge dazu:

- **Der Shunt drückt den Strom um rund 5 %.** Der LED-Kreis sieht 190 Ω statt 180 Ω, aus 18,3 mA werden 17,4 mA. Der echte Wert liegt also über dem gemessenen, um den Faktor `(180 + 10) / 180`, sofern die 180 Ω des Mainboards ihrem Aufdruck entsprechen.
- **In Pin 1 gemessen wäre das Plateau um bis zu 2,65 mA zu hoch**, weil dort der Fototransistorzweig mitläuft und sein Beitrag mit der Reflexion wächst. In Pin 3 fließt allein der LED-Strom.

Die Flussspannung fällt am selben Aufbau mit ab, aus zwei Tastkopfablesungen in der hellen Halbwelle. Sie trägt den Verstärkungsfehler des Geräts zweifach und taugt deshalb zur Gegenprobe, nicht zur Entscheidung:

```
V_F = V(Pin 1) − V(Pin 3, Sensorseite)
```

### Weg 2: Emitter dauerhaft an, an der Maschine

**Nur Pin 3 vom Mainboard trennen.** Pin 1 und Pin 2 bleiben gesteckt, die Maschine läuft und versorgt den Sensor aus ihrer eigenen 5-V-Rail. Pin 3 geht stattdessen über 180 Ω auf die Masse der Maschine.

```
Pin 1  ──  Mainboard, unverändert
Pin 2  ──  Mainboard, unverändert
Pin 3  ──►  180 Ω  ──►  GND der Maschine
                        Multimeter auf Gleichspannung über den 180 Ω
```

Der 180 Ω des Mainboards liegt damit nicht mehr im Kreis, denn Pin 3 ist dort abgehängt. Die Masse muss die der Maschine sein, sonst schließt sich kein Kreis und es fließt nichts.

Die Spannung über dem 180 Ω geteilt durch seinen gemessenen Wert ist der Strom, ohne Bürdenspannung und ohne den Kreis auftrennen zu müssen. Zwei Dinge dazu:

- Q1 fehlt in diesem Pfad, seine rund 0,2 V also auch, weshalb hier `0,2 V / 180 Ω` = 1,1 mA mehr herauskommen als die Maschine selbst treibt.
- Die LED leuchtet dauerhaft statt mit 50 % Tastverhältnis, 19,4 mA gegen einen Dauergrenzwert von 50 mA. Die Maschine sieht auf diesem Kanal ständig eine Reflexion und meldet dort einen Ball. Für die Messung ohne Belang.

Die Flussspannung fällt mit ab: Multimeter auf Gleichspannung, Pin 1 gegen Pin 3.

### Auswertung

| Ergebnis | Bedeutung |
|---|---|
| Beide Wege liefern denselben Strom, ±1,1 mA | Die Rekonstruktion des Mainboards ist belegt, samt der 180 Ω aus dem Aufdruck und der 0,2 V für Q1 |
| Die Wege weichen deutlich ab | Einer der beiden nicht gemessenen Werte ist falsch. Der vierte, nicht abgelesene Widerstand des Mainboards ist der offene Kandidat |
| V_F bis 1,4 V beim gemessenen Strom | Figure 3 gilt, die Stromwerte der Auslegung stehen |
| V_F über 1,4 V | Das Datenblatt deckt das Bauteil nicht. Jeder Emitterstrom der Auslegung ist mit dem gemessenen V_F neu zu rechnen, denn er steht als `(V − V_F) / R` darin |

Über den Emitterwiderstand der geplanten Kanäle entscheidet diese Messung nicht. Die Stockmaschine wertet Pin 2 gegen eine Basis-Emitter-Schwelle aus und braucht deshalb eine andere Signalgröße als ein Kanal mit Zehn-Bit-Wandler und Kalibrierung je Kanal. Was der geplante Kanal braucht, sagt Messung B.

## Aufbau für B, C und D

**Der Sensor sitzt an seinem Einbauplatz in der Maschine**, im endgültigen Abstand über der Bahn. Der Hintergrund geht in jede Ablesung ein: "freie Bahn" ist die Reflexion des Holzes, und ohne die echte Bahn hinter der Kugelposition wird der Raum gemessen statt der Kanal. Die Maschine bleibt dabei stromlos, ihr Stecker an Pin 1 bis 3 ist abgezogen, und die Bank versorgt den Sensor allein. Läuft sie, dann tragen ihre übrigen IR-Emitter mit 333 Hz und ihre Stockbeleuchtung in jede Ablesung hinein, und beides gibt es in der fertigen Maschine nicht mehr: bei 333 Hz löscht die Differenzbildung des Treibers fast nichts aus, `1 − cos(2π f × 600 µs)` ist dort 0,69 gegen 0,070 bei 100 Hz.

```
3,3 V  ────────────────►  Pin 1

Pin 2  ──┬─────────────►  4,7 kΩ  ──►  GND
         │
         └──────────────  MESSKNOTEN, hier misst Multimeter bzw. Tastkopf

Pin 3  ──►  220 Ω  ────►  loser Draht
                          auf GND getippt:  LED leuchtet
                          abgehoben:        LED dunkel
```

Der LED-Strom stellt sich auf 9,4 mA ein, aus 3,3 V minus 1,24 V Flussspannung bei diesem Strom, Figure 3 des Datenblatts, geteilt durch 220 Ω. Das ist der Nennwert der späteren Schaltung, dort um die 8 mV Kanalwiderstand von Q1 tiefer.

**Der Strom wird nicht angenommen, sondern mitgemessen.** Multimeter über den 220 Ω, LED an:

```
I_LED = U(220 Ω) / R gemessen
V_F   = 3,3 V − U(220 Ω)
```

Damit steht die Flussspannung ein zweites Mal, bei 9 mA statt bei den 18 mA aus Messung A, und die beiden Punkte spannen die Kurve auf, aus der die Auslegung ihre Ströme zieht.

### Vor der ersten Messung

**Spannung prüfen, bevor der Sensor angeschlossen wird.** 3,3 V, nicht 5 V. Bei 5 V stimmen der LED-Strom und sämtliche Ablesewerte nicht mehr; zerstört wird nichts, 16,8 mA liegen innerhalb der 50 mA des Bauteils.

**Funktionsprobe.** Draht auf GND legen, Multimeter am Messknoten. Eine Hand vor den Sensor gehalten muss den Wert deutlich bewegen. Bewegt sich nichts, ist die Verdrahtung falsch.

## Messung B: Was eine Kugel zurückgibt

Multimeter am Messknoten gegen GND, Gleichspannung. Beleuchtung wie im Betrieb, denn die beiden Ablesungen mit dunkler LED sind gleichzeitig der Gleichanteil, den Messung D auswertet.

**Die Kugel liegt in der Bahn, an der Stelle über dem Sensor**, nicht an dessen Stirnfläche. Der Abstand ist der einzige Weg, auf dem die Montage in das Signal kommt, und an der Stirnfläche fällt es um ein Mehrfaches zu hoch aus.

Vier Werte in einem Zug aufnehmen, ohne die Beleuchtung anzurühren, weil die vier gegeneinander verrechnet werden und eine Drift dazwischen genau in die Differenz fällt. Danach das Paar `Kugel, LED an` und `freie Bahn, LED an` einmal wiederholen: weichen die beiden Durchgänge um mehr als ein paar Millivolt ab, hat sich das Umgebungslicht bewegt und die Aufnahme zählt nicht.

Vier Werte aufnehmen:

| | freie Bahn | Kugel |
|---|---|---|
| Draht auf GND, LED an | | |
| Draht abgehoben, LED aus | | |

**Auswertung.** In jeder Spalte den unteren Wert vom oberen abziehen, dann die beiden Ergebnisse voneinander abziehen. Diese Zahl ist das Signal, das ein Kanal zur Verfügung hat. Erwartet werden rund 235 mV, was der Annahme von 50 µA an 4,7 kΩ entspricht.

**Die Dunkelspalte wird zweimal ausgewertet.** Ihre Differenz, `Kugel, LED aus` minus `freie Bahn, LED aus`, sagt in beiden Richtungen, ob die Kugel die Dunkelablesung selbst verschiebt: sie reflektiert Umgebungslicht herunter und schattet es gleichzeitig ab.

| Verschiebung | Bedeutung |
|---|---|
| unter 3,2 mV, einem Wandlerschritt | Die Dunkelphasen vor und nach der hellen bleiben gültig, auch wenn die Kugel dazwischen kommt oder geht. Ein Wertepaar braucht die Kugel nur über den beiden hellen Phasen |
| darüber | Die Dunkelbasis gilt nur mit Kugel. Ein bestätigtes Wertepaar braucht sie über allen fünf Phasen |

**Das ist der eingeschwungene Wert.** Die Firmware liest 280 µs nach dem Phasenwechsel, und was sie dort sieht, ist dieser Wert mal dem Hub, den τ zulässt: 98 % bei τ = 62 µs, 49 % bei τ = 214 µs. Messung C liefert den Faktor, erst beide zusammen ergeben das Signal am Leseinstant.

| Ergebnis | Bedeutung |
|---|---|
| deutlich unter 235 mV | Es kommt zu wenig Licht zurück, die Auslegung braucht einen helleren Emitter |
| Ablesewert nähert sich 2,47 V | Der Kanal hat keinen Spielraum mehr, der Pull-down muss kleiner werden. 2,47 V ist die Decke des Teilers, `3,3 V × 4,7 / (1,585 + 4,7)`, nicht die des Wandlers |
| dazwischen | Die Auslegung steht |

## Messung C: Die Zeitkonstante τ

Multimeter abnehmen, Tastkopf an denselben Messknoten, Masseklemme an GND.

```
Kopplung    DC
Zeitbasis   100 µs/div
Trigger     Einzelschuss, fallende Flanke
Pegel       auf halber Höhe zwischen den beiden Werten der Kugelspalte aus Messung B
```

**Tastkopf vorher am Kalibriersignal des Oszilloskops kompensieren.** Dessen Rechteck liegt bei 1 kHz, also 500 µs pro Pegel, mitten im gesuchten Bereich von 62 bis 214 µs. Ein verstellter Trimmer verbiegt die Exponentiale zu Über- oder Unterschwingen, und der daraus abgelesene Wert sieht sauber aus.

**Die Kugel liegt dabei in der Bahn über dem Sensor.** Der Sprung ist dann groß genug für eine saubere Auswertung, und er entsteht an dem Arbeitspunkt, der zählt.

### Zuerst die fallende Flanke

Draht fest auf GND halten, dann abheben. Kontakt trennen prellt nicht, Kontakt herstellen schon, deshalb diese Richtung zuerst.

Die Geschwindigkeit der Bewegung spielt keine Rolle. Sobald der Kontakt öffnet, ist die LED innerhalb von Nanosekunden aus, und was das Oszilloskop zeigt, ist ausschließlich die Trägheit des Sensors.

### Danach die steigende Flanke

Trigger auf steigende Flanke umstellen. Den Draht langsam und fest gegen GND drücken, nicht antippen und nicht darüberstreichen, denn ein fester Druck prellt am wenigsten. Prellt es dennoch, neu bewaffnen und wiederholen. Ein Rhythmus ist nicht nötig, jede Aufnahme steht für sich.

### Brauchbar oder nicht

| Brauchbar | Verwerfen |
|---|---|
| eine durchgehende Rampe | Stufen oder Kerben in der Rampe |
| Kurve startet auf dem Ruhewert | Kurve startet irgendwo in der Mitte |

### Ablesen

Zwei gleichwertige Wege, einer davon genügt:

| Verfahren | Ergebnis |
|---|---|
| Cursor auf 63 % der Sprunghöhe | Die Zeit bis dorthin ist τ |
| Anstiegszeit von 10 auf 90 % messen | τ = t / 2,2 |

Beide Flanken getrennt notieren. Sie müssen nicht gleich sein, und falls sie es nicht sind, ist genau das die interessante Information.

### Einordnung der Werte

Die beiden Kandidaten stammen aus demselben Datenblattmaximum von 100 µs Antwortzeit bei R_L = 1 kΩ, einmal mit dem
Kurvenverlauf von Figure 6 auf 4,7 kΩ gebracht und einmal linear skaliert. Die Kurve selbst ist ein Typwert und liegt
darunter: 71 µs Antwortzeit bei 4,7 kΩ, also τ = 32 µs.

| τ liegt bei | Bedeutung |
|---|---|
| 30 bis 65 µs | Figure 6 gilt. 32 µs ist ihr Typwert, 62 µs derselbe Verlauf auf das Maximum angewandt. Kurze Messzyklen sind möglich |
| 180 bis 250 µs | Die lineare Skalierung gilt, 214 µs. Die Auslegung ist darauf gebaut und steht unverändert |
| dazwischen | Einen zweiten Sensor messen, das trennt Bauteilstreuung von der Wahrheit |
| beide Flanken deutlich verschieden | Gesondert vermerken, das Rechenmodell kennt bisher nur einen Wert für beide Richtungen |

### Was B und C über das Bauteil selbst sagen

Die Kennung als Sharp GP2S700HCP steht über Gehäusebild und Padbelegung, die Aufschrift war nicht lesbar. Jede
Datenblattzahl der Auslegung hängt daran: V_F aus Figure 3, die Antwortzeit aus Figure 6, die 50 mA I_F und die
20 mA I_C. Drei Ablesungen prüfen sie an unabhängigen Stellen.

| Ablesung | Erwartung |
|---|---|
| V_F bei 9 mA, aus B | 1,25 V, 25-°C-Kurve von Figure 3 |
| τ bei 4,7 kΩ, aus C | 32 bis 62 µs nach Figure 6 |
| Fotostrom, aus B | 50 µA sind die Annahme, die I_C-Spanne des Datenblatts deckt sie ab |

Verfehlen V_F und τ gemeinsam, ist es ein anderes Bauteil. Dann wird das Datenblatt gesucht, das zu den gemessenen
Werten passt, statt einzelne Zahlen der Auslegung nachzuziehen.

## Messung D: Das Umgebungslicht

Gemessen werden die Lampen und der Ort. Der Treiber zieht von jedem Hellwert den Mittelwert der beiden Dunkelwerte davor und danach ab. Ein konstantes Umgebungslicht fällt dabei exakt heraus, ein flackerndes nur teilweise, und wie viel stehen bleibt, hängt an der Modulationstiefe der Lampen.

Drei Vorgaben gelten für die ganze Messung:

- **Der Draht an Pin 3 bleibt abgehoben.** Die LED ist aus, der Sensor sieht ausschließlich das Umgebungslicht.
- **Sensor an seinem Einbauplatz**, Zimmerlicht wie im Betrieb.
- **Still halten.** Eine Bewegung vor dem Sensor verschiebt den Wert um ein Mehrfaches dessen, was eine Kugel erzeugt.

**Die Beleuchtung der Maschine ist heute nicht messbar.** Sie wird durch das eigene Lighting-Subsystem ersetzt, das noch nicht existiert, und die Lampen der Stockmaschine sind kein Ersatz dafür: adressierbare RGB-LEDs strahlen im IR-Band anders und dimmen mit der PWM ihres Treiberchips. Von den 80 Schritten, die die Auslegung annimmt, deckt diese Messung die 60 aus dem Raum. Die 20 aus der Maschine bleiben offen und werden zur Anforderung an das Lighting: keine Dimmfrequenz im Kilohertzbereich in Sichtweite eines Sensors, aus dem Grund, der unten in der Auswertung steht.

### Schritt 1: Der Gleichanteil

Das ist die Ablesung "freie Bahn, LED aus" aus Messung B. Die Auslegung nimmt 258 mV an, 80 Schritte des Zehn-Bit-Wandlers zu je rund 3,2 mV.

### Schritt 2: Der Wechselanteil

Multimeter abnehmen, Tastkopf an denselben Messknoten, Masseklemme an GND.

```
Kopplung    AC. Ohne sie schiebt der Gleichanteil die Kurve aus dem Bild
Zeitbasis   2 ms/div, das zeigt zwei Perioden bei 100 Hz
Vertikal    20 mV/div, danach so weit aufdrehen, wie das Bild zulässt
Trigger     Auto, Pegel in die Mitte der Welligkeit
Mittelung   an, 16 Durchläufe oder mehr
```

Die Mittelung ist nötig, weil die Welligkeit bei 100 Hz getriggert stehenbleibt und das Rauschen nicht. Ohne sie greift die V_pp-Messfunktion die höchste Rauschspitze und der Rauschboden fällt zu hoch aus.

Zwei Werte ablesen:

| Ablesung | Wie |
|---|---|
| V_pp der Welligkeit | Messfunktion des Geräts, sonst Cursor auf den höchsten und den tiefsten Punkt |
| Frequenz der Welligkeit | Messfunktion, sonst Periodendauer ablesen und umrechnen. Erwartet werden 100 Hz |

### Schritt 3: Die Gegenprobe

Zimmerlicht aus, Aufnahme wiederholen. Die Welligkeit muss weitgehend verschwinden. Bleibt sie stehen, stammt sie aus dem Aufbau, also aus Netzteil, Masseführung oder Tastkopf, und Schritt 2 zählt erst wieder, wenn die Ursache gefunden ist.

Damit die Gegenprobe eindeutig ist, muss jede andere Lichtquelle mit aus, die Beleuchtung der Maschine eingeschlossen. Eine stehenbleibende Welligkeit ist sonst nicht zuzuordnen.

### Auswertung

```
amp   = V_pp / 2
Rest  = amp × (1 − cos(2π × f × 600 µs)),   f aus Schritt 2
                                            600 µs ist die Phasenlänge des Treibers
```

Der Ausdruck ist der schlechteste Fall über die Lage der Welligkeit. Vollständig lautet er `amp × cos(2π f φ) × (1 − cos(2π f × 600 µs))`, mit φ als Abstand der hellen Abtastung vom Wellenberg, und er wird maximal, wenn die helle Abtastung genau auf den Berg fällt.

Bei 100 Hz ist der Klammerausdruck 0,070, der Rest also ein Vierzehntel von amp. Die Auslegung nimmt amp = 116 mV an und rechnet mit 8,1 mV Rest.

| Ergebnis | Bedeutung |
|---|---|
| V_pp bis 230 mV bei 100 Hz | Wie angenommen, die Auslegung steht |
| V_pp deutlich darüber | Der Rauschboden liegt über der Annahme. Die Phase muss kürzer werden, denn der Rest fällt quadratisch mit ihr |
| Welligkeit nicht bei 100 Hz, sondern bei einigen kHz | PWM-gedimmte Lampen. Ab 833 Hz, wo `2π f × 600 µs` gerade π erreicht, löscht die Differenzbildung nichts mehr aus und der Rest erreicht 2 × amp. Andere Lampe über der Maschine, und die Beleuchtung der Maschine selbst nicht mit einigen kHz dimmen |
| Gleichanteil deutlich über 258 mV | Das Umgebungslicht verbraucht mehr Spielraum als angenommen. Bei 2,47 V ist er ganz weg, das ist die Decke aus Messung B |

## Messung E: Die Referenzkonstante k

Die Wandler messen gegen die Versorgungsspannung der Platine. In der hellen Phase liegt diese Spannung etwas tiefer als in der dunklen, weil der LED-Bus sie belastet. Ohne Korrektur bleibt deshalb ein Rest des Umgebungslichts in der Differenz stehen. Die Firmware rechnet `Wert = hell − k × dunkel`, und k ist das Verhältnis der beiden Spannungen.

**Wie genau k sein muss, sagt Messung D.** Ein Fehler δk hinterlässt `δk × dunkel` im Ergebnis, mit `dunkel` in Wandlerschritten:

```
Genauigkeit   δk ≤ 1 / dunkel        bei 80 Schritten:  1,25 %
lohnt sich    (k − 1) × dunkel ≥ 1   bei 80 Schritten:  k − 1 ≥ 1,25 %
```

Fällt die Reglerdifferenz kleiner aus, kostet die Korrektur weniger als einen Schritt und k bleibt 1. Der Gleichanteil aus Messung D setzt beide Schwellen, und er kann bis an die Decke von 2,47 V reichen, wo aus 80 Schritten 760 werden und k auf 0,13 % genau sein muss.

### Erster Teil: Taugt der Regler

Steckbrett, kein Sensor beteiligt. Nur der D24V5F3, zwei Widerstände und das Multimeter.

**Aufbau.** 5 V an den Eingang des Reglers. Multimeter auf Gleichspannung an den Ausgang, mit 1 mV Auflösung, also nicht im 20-V-Bereich. Die beiden Widerstände wechselweise vom Ausgang nach GND.

| Widerstand | Entspricht | Strom |
|---|---|---|
| 180 Ω | dunkle Phase, 17,1 mA nach der Auslegung | 18 mA |
| 18 Ω, als zwei 36 Ω parallel | helle Phase bei sechzehn Kanälen, 182,1 mA | 183 mA |

Fünfmal zwischen beiden wechseln und jedes Mal beide Spannungen notieren, **die Eingangsspannung mit**. Bricht die 5-V-Quelle bei 183 mA selbst ein, trägt die Linienregelung des Moduls einen Teil davon in den Quotienten, und k gilt dann für die Quelle statt für die Last. Vor dem Ablesen eine Sekunde warten: der Einschwingvorgang der Laststufe ist in [`design.md`](design.md#appendix-derivations) gesondert gerechnet und nach 200 µs abgeklungen, während die Firmware erst 280 µs nach dem Phasenwechsel liest.

**Die Differenz direkt ablesen, nicht zwei Absolutwerte.** Meter am 180-Ω-Zweig mit der Relativfunktion nullen, dann auf 18 Ω umstecken und die Differenz ablesen. Zwei Absolutwerte zu je ±1 mV geben eine Differenz von etwa 30 mV nur auf ±2 mV, und das sind 6 % des Wertes, auf den es ankommt.

| Ergebnis | Bedeutung |
|---|---|
| Beide Ablesungen wiederholen sich auf 1 bis 2 mV | Die Differenz ist reproduzierbar, der Regler taugt |
| Eine Ablesung springt bei gleicher Last zwischen zwei Niveaus | Das fängt keine Konstante auf, es braucht einen anderen Regler |

Dass der Regler zwischen den Phasen die Betriebsart wechselt, ist erwartet und kein Fehler. Die Grenze zum lückenden Betrieb liegt beim D24V5F3 bei 51 mA, die dunkle Phase mit 17 mA darunter und die helle mit 182 mA darüber, beide um mehr als den Faktor drei entfernt. Solange jede Phase immer dieselbe Betriebsart erwischt, steckt der Unterschied in k.

### Zweiter Teil: Der Wert für die Firmware

**k wird am fertigen Board mit seinem endgültigen Kabel gemessen.** In der Differenz steckt neben der Lastregelung des Moduls der Spannungsabfall der 3,3-V-Zuleitung, und der gehört zur Installation, nicht zum Bauteil. 165 mA über einen halben Meter dünner Litze fallen in der Größenordnung von 20 mV ab, also so viel wie die Lastregelung des Moduls selbst. Am Steckbrett ist dieser Anteil nicht enthalten.

Gemessen wird deshalb am Versorgungsknoten des Boards, mit dem Kabel, das dort verbaut wird, und mit dem LED-Bus als Last statt der Widerstände.

```
k = U(dunkle Phase) / U(helle Phase)
```

## Wie viele Sensoren

**Messung B an allen drei Sensoren.** Der Fototransistor streut laut Datenblatt über Faktor 6,8, und ob die vorhandenen Exemplare nah beieinander oder weit auseinander liegen, verrät ein einzelner Sensor nicht.

**Messung C an einem Sensor.** τ hängt am Bauteiltyp und am Pull-down, nicht an der Montage, und die Frage lautet 62 oder 214 µs. Landet der Wert dazwischen, etwa bei 100 bis 150 µs, dann einen zweiten Sensor messen, um Bauteilstreuung von der Wahrheit zu trennen.

**Messung D an einem Sensor, an seinem Einbauplatz.** Gemessen werden die Lampen und der Ort, und beide sind für alle Kanäle dieselben. Sitzen später Sensoren an merklich unterschiedlich hellen Stellen, gilt der hellste.

Damit der Vergleich der drei Exemplare etwas taugt:

- **Alle drei an derselben Stelle**, nacheinander in dieselbe Aufnahme gesteckt. Sonst wird die Montage gemessen und nicht das Bauteil.
- **Sensoren beschriften**, damit sich später zuordnen lässt, welches Exemplar welchen Wert geliefert hat.

## Ergebnisbogen

```
Sensor-Kennung                       ________
Versorgungsspannung, gemessen        ________ V
Pull-down, gemessen                  ________ kΩ
Emitterwiderstand, gemessen          ________ Ω
Abstand Sensor zur Bahn              ________ mm

MESSUNG A, Weg 1, Maschine laeuft, Oszilloskop
10 Ω, gemessen                        ________ Ω
Plateau über 10 Ω, helle Halbwelle    ________ mV
daraus Strom                          ________ mA
korrigiert um (180+10)/180             ________ mA
Tastverhältnis                        ________ %
Pin 1 gegen GND                       ________ V
Pin 3 gegen GND, helle Halbwelle      ________ V
daraus V_F                            ________ V

MESSUNG A, Weg 2, Pin 3 ueber 180 Ohm auf GND
180 Ω, gemessen                       ________ Ω
Spannung über dem 180 Ω               ________ mV
daraus Strom                          ________ mA
abzüglich 1,1 mA für Q1               ________ mA
Versorgung, Pin 1 gegen GND           ________ V
V_F, Pin 1 gegen Pin 3                ________ V

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
τ, steigende Flanke                  ________ µs

MESSUNG D
Gleichanteil, Licht wie im Betrieb   ________ mV
V_pp der Welligkeit                  ________ mV
Frequenz der Welligkeit              ________ Hz
V_pp bei dunklem Raum                ________ mV

MESSUNG E, erster Teil, Steckbrett
U bei 180 Ω     1 ___ 2 ___ 3 ___ 4 ___ 5 ___ mV
Differenz zu 18 Ω, relativ gemessen
                1 ___ 2 ___ 3 ___ 4 ___ 5 ___ mV
Eingang bei 180 Ω                     ________ V
Eingang bei 18 Ω                      ________ V
Betriebsart springt bei gleicher Last   ja / nein

MESSUNG E, zweiter Teil, fertiges Board
U, dunkle Phase                       ________ mV
U, helle Phase                        ________ mV
k                                     ________
```
