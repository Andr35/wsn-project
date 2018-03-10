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

#### Evaluation

Save log file in cooja an then:

```sh
$ python parse-stats.py loglistener.txt
```
