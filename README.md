# wsn-project
Wireless Sensor Network Project


#### Report

- data collection protocol: handle path metric and beacon seq num overflow
- handle loops


#### Compile and run

```sh
$ make
$ cooja test.csc
```
```sh
$ cooja_nogui test_nogui_dc.csc
```

#### Evaluation

Save log file in cooja an then:

```sh
$ python parse-stats.py loglistener.txt
```

### Notes

- SLIDE 11 - Topology reports -> in piggybacking usa un header flessibile per aggiungere tutti i nodi che hanno fatto forward del messaggio (+ nell'header aggiungi la sua lunghezza per decodificarlo).

- SLIDE 11 - Dedicated topology report vs piggybacking: deve essere bilanciato in base a quanti dati passano -> il root/sink invia un beacon -> il nodo usa un timer random prima di inviare il dedicated report per evitare floodings.
-> gestire l'invio (decidere) del report. Ogni volta che un msg parte (report o applicaiton message) setta un timer -> non inviare altri reports prima che il timer scada.

- consider (beacon.seqn > conn->beacon_seqn) and (beacon.seqn == conn->beacon_seqn) separately -> cosi' il nodo puo' anche essere mosso
  perche' fa maggior affidamento a seqn piu' freschi

- solo quando il parent cambia -> invia specifico topology report (usa un timer per inviarlo un po' dopo il timer)
  --> riconoscere messaggio "topology report": avra' datalen=0 dopo hdrreduce (?)
  NB: appunti per report: anche senza topology report dedicato, i risultati sono ottimi:

    ----- Data Collection Overall Statistics -----
    Total Number of Packets Sent: 531
    Total Number of Packets Received: 529
    Overall PDR = 99.62%
    Overall PLR = 0.38%
    ----- Source Routing Overall Statistics -----
    Total Number of Packets Sent: 172
    Total Number of Packets Received: 172
    Overall PDR = 100.00%
    Overall PLR = 0.00%

- con Duty Cycle -> 80% dc

- Come evitare collisioni? (cooja -> blue transmit, green: received, red: failure or collision)

- Evitare loops:
    - dal sink al nodo -> ok, il sink calcola prima se c'e' un loop
    - vicversa -> il nodo non lo puo' sapere. Pero' se nel msg c'e' la lista di  nodi incontrati, questo il loop essere evitato facendo controlli sulla lista.

- sr_send = sourceRouting_send

- Beacon in arrivo: a parita' di metric controllare se l'rssi e' migliore per avere comunque un link piu' "stabile" e "sicuro".
