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
                       ______________________________
                      |   L T L T L T L T L T L T    |
                      |                              |
                   RST|                             1|TX HSer
                    A0|                             3|RX HSer
                    D0|16                           5|D1
                    D5|14                           4|D2
                    D6|12                    10kPUP_0|D3
  RX SSer/HSer swap D7|13                LED_10kPUP_2|D4
  TX SSer/HSer swap D8|15                            |GND
                   3V3|__                            |5V
                         |                           |
                         |___________________________|


         _______________       
        |               |      
        |  Victron JST  |      
        |_______________|      
        |YEL|WHT|RED|BLK|      
        | 1 | 2 | 3 | 4 |      
        |+5V|TX |RX |GND|      
        |___|_:::::_|___|      
                              


  D1 mini D7 -> > LV1 / TX
  GND -> GND

  Get values via WebUI
  http://<IPADDR>

  Get values via JSON
  GET http://<IPADDR>/rest

  Get report via Telnet
  Telenet <IPADDR>

```
