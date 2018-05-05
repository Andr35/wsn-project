# wsn-project
Wireless Sensor Network Project


#### Report

- data collection protocol: handle path metric and beacon seq num overflow
- handle loops

#### Change RDC

In `project-conf.h` change `NETSTACK_RDC`;

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

See `nodes.MD`


### Final result

#### RDC NullRDC

```
----- Data Collection Overall Statistics -----
Total Number of Packets Sent: 531
Total Number of Packets Received: 527
Overall PDR = 99.25%
Overall PLR = 0.75%

----- Source Routing Node Statistics -----
Node 2: TX Packets = 20, RX Packets = 20, PDR = 100.00%, PLR = 0.00%
Node 3: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 4: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 5: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 6: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 7: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 8: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 9: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 10: TX Packets = 19, RX Packets = 18, PDR = 94.74%, PLR = 5.26%

----- Source Routing Overall Statistics -----
Total Number of Packets Sent: 172
Total Number of Packets Received: 171
Overall PDR = 99.42%
Overall PLR = 0.58%
```

#### RDC ContikiMAC

```
----- Data Collection Node Statistics -----
Node 2: TX Packets = 59, RX Packets = 58, PDR = 98.31%, PLR = 1.69%
Node 3: TX Packets = 59, RX Packets = 55, PDR = 93.22%, PLR = 6.78%
Node 4: TX Packets = 59, RX Packets = 47, PDR = 79.66%, PLR = 20.34%
Node 5: TX Packets = 59, RX Packets = 53, PDR = 89.83%, PLR = 10.17%
Node 6: TX Packets = 59, RX Packets = 57, PDR = 96.61%, PLR = 3.39%
Node 7: TX Packets = 59, RX Packets = 55, PDR = 93.22%, PLR = 6.78%
Node 8: TX Packets = 59, RX Packets = 45, PDR = 76.27%, PLR = 23.73%
Node 9: TX Packets = 59, RX Packets = 55, PDR = 93.22%, PLR = 6.78%
Node 10: TX Packets = 59, RX Packets = 55, PDR = 93.22%, PLR = 6.78%

----- Data Collection Overall Statistics -----
Total Number of Packets Sent: 531
Total Number of Packets Received: 480
Overall PDR = 90.40%
Overall PLR = 9.60%

----- Source Routing Node Statistics -----
Node 2: TX Packets = 20, RX Packets = 20, PDR = 100.00%, PLR = 0.00%
Node 3: TX Packets = 20, RX Packets = 19, PDR = 95.00%, PLR = 5.00%
Node 4: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 5: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 6: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 7: TX Packets = 19, RX Packets = 18, PDR = 94.74%, PLR = 5.26%
Node 8: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 9: TX Packets = 19, RX Packets = 19, PDR = 100.00%, PLR = 0.00%
Node 10: TX Packets = 19, RX Packets = 17, PDR = 89.47%, PLR = 10.53%

----- Source Routing Overall Statistics -----
Total Number of Packets Sent: 173
Total Number of Packets Received: 169
Overall PDR = 97.69%
Overall PLR = 2.31%
```
