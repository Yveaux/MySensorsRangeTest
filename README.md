# MySensorsRangeTest
Range test for 2 nodes using the MySensors library

# Installation
Required libraries:
* MySensors - development: https://github.com/mysensors/MySensors/tree/development
* AnsiStream: https://github.com/neu-rah/AnsiStream
* Streaming: https://github.com/scottdky/Streaming

# Deploy
Prepare two nodes with nRF24 radios.
On the top of the MySensorsRangeTest.ino sketch you'll find two defines, MASTER or SLAVE.
Flash this sketch to one node with SLAVE #defined and to the other with MASTER #defined.

# Running
Connect to the MASTER node with a serial terminal supporting ANSI escape sequences (e.g. Tera Term or Putty on Windows).

<img src="https://raw.githubusercontent.com/Yveaux/MySensorsRangeTest/master/images/screenshot.png" alt="Range test screenshot">



