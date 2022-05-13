# esp8266-victron-mppt-solarchargecontroller
This is a online information server with rest api based on esp8266 and Victron Solar MPPT Solar Charge Controller

## Hardware
The following hardware is required:
```
- D1 mini (ESP8266)
- Level Shifter 3.3V / 5V
- Victron MPPT Charge Controller (like SmartSolar 100/30)
```

## Connection

D1 mini -> Level Shifter 3.3V / 5V
```
D7 / RX -> LV1 / TXI
GND -> GND
3V3 -> LV
```

Level Shifter 3.3V / 5V -> JST Port am Victron SmartSolar
```
HV1 / TX0 -> 2 (TX / White)
GND -> 4 (GND / BLack)
D1 mini -> MAX485
```

![alt text](https://github.com/datjan/esp8266-victron-mppt-solarchargecontroller/blob/main/connection-schema.png?raw=true)

## Development
This sketch is for following development environment
```
Arduino
```

Following libraries are required
```
https://github.com/me-no-dev/ESPAsyncTCP
https://github.com/me-no-dev/ESPAsyncWebServer
```

## Setup
Setup wifi in esp8266-victron-mppt-solarchargecontroller.ino:
```
const char* wifi_ssid = "xxx";
const char* wifi_password = "xxx";
```

Setup victron device in config.h:
```
#define MPPT_100_30    // Define used Victron Device
```

Actual Supported:
- "MPPT 75 | 10"
- "MPPT 75 | 15" tested with FW 1.56
- "MPPT 100 | 20" tested with FW 1.5 / 1.56
- "MPPT 100 | 30" tested with FW 1.59

## Upload to device
Following files needs to be uploaded to the ESP8266 (D1 mini)
```
esp8266-victron-mppt-solarchargecontroller.ino
index_page.h
config.h
```

## Show info
Access webpage on http://[IPADDR]
![alt text](https://github.com/datjan/esp8266-victron-mppt-solarchargecontroller/blob/main/img_webpage.png?raw=true)
  
  
Access rest api on GET http://[IPADDR]/rest
![alt text](https://github.com/datjan/esp8266-victron-mppt-solarchargecontroller/blob/main/img_restapi.png?raw=true)
  
