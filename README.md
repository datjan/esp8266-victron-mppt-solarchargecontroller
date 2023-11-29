# esp8266-victron-mppt-solarchargecontroller

This is a modification of the online information server with rest api based on esp8266 and Victron Solar MPPT Solar Charge Controller
from https://github.com/datjan/esp8266-victron-mppt-solarchargecontroller.

This version does not use SoftwareSerial. 
instead, the serial data  incoming at d7 is read using Serial.swap(), which swiches the hardware serial UART to D7/D8

Instead of using the serial port and the serial monitor, the information is routed to telnet, where the stream is printed.

## Advantages of this solution: 
- SoftwareSerial is a performance killer. Running over the hardware UART is ways faster and uses far less resources. 
- Telnetstream is much more flexible.
- The overal solution is 100% wireless, you don't need any connection between the computer and the Solar installation.
- This avoids galvanic problems and is ways mor robust against thunderbolt strikes
- You can us as many ESP devices as you want each one has its own IP and can be monitored from separate Telnet instances.


## Hardware
The following hardware is required:
```
- D1 mini (ESP8266)
- Direct connection without level shifter
- Victron MPPT Charge Controller (like SmartSolar 100/30)
- Micro buck converter from battery voltage to 5V
```

## Connection
  D1 mini D7 -> > LV1 / TX  (no need for a level shifter, the Tx output of Victron is very weak and is easily clipped to 3,3V)
  GND -> GND

  Get values via WebUI
  http://<IPADDR>

  Get values via JSON
  GET http://<IPADDR>/rest

  Get report via Telnet


```
$ telnet 192.168.188.28
Trying 192.168.188.28...
Connected to esp-7b1476.fritz.box.
Escape character is '^]'.
BatV:13.8000 BatI:0.2100 PanV: 18.7900 PanW: 0.0030, LoadI 0.0000
BatV:13.8000 BatI:0.2200 PanV: 18.8100 PanW: 0.0030, LoadI 0.0000
BatV:13.8000 BatI:0.2100 PanV: 18.7900 PanW: 0.0030, LoadI 0.0000
BatV:13.8000 BatI:0.2100 PanV: 18.8100 PanW: 0.0030, LoadI 0.0000

Q

bye bye
Connection closed by foreign host.
```

