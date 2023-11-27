/*  WEMOS D1 Mini
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


         _______________        -----------------------
        |               |      |TX0|RXI|HV |GND|RXI|TX0|
        |  Victron JST  |      |HV1|HV2|   |   |HV3|HV4|
        |_______________|      |-----------------------|
        |YEL|WHT|RED|BLK|      | LEVEL SHIFTER 3.3V/5V |
        | 1 | 2 | 3 | 4 |      |-----------------------|
        |+5V|TX |RX |GND|      |LV1|LV2|   |   |LV3|LV4|
        |___|_:::::_|___|      |TXI|RX0|LV |GND|RX0|TXI|
                                -----------------------


  D1 mini -> Level Shifter 3.3V / 5V
  D7 / RX -> LV1 / TXI
  GND -> GND
  3V3 -> LV

  Level Shifter 3.3V / 5V -> JST Port am Victron SmartSolar
  HV1 / TX0 -> 2 (TX / White)
  GND -> 4 (GND / BLack)


  Get values via WebUI
  http://<IPADDR>

  Get values via JSON
  GET http://<IPADDR>/rest

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include <ESPAsyncTCP.h>          // https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncWebServer.h>    // https://github.com/me-no-dev/ESPAsyncWebServer

//#include <SoftwareSerial.h>
#include <TelnetStream.h>

#include "config.h"
#include "index_page.h"
#include "secret.h"

//--------------------------- SETUP -------------------------------------

#define rxPin D7
#define txPin D8                                    // TX Not used

String devicename = "Victron_Logger";


//--------------------------- SETUP -------------------------------------

AsyncWebServer server(80);

//SoftwareSerial victronSerial(rxPin, txPin);         // RX, TX Using Software Serial so we can use the hardware serial to check the ouput
// via the USB serial provided by the NodeMCU.
char receivedChars[buffsize];                       // an array to store the received data
char tempChars[buffsize];                           // an array to manipulate the received data
char recv_label[num_keywords][label_bytes]  = {0};  // {0} tells the compiler to initalize it with 0.
char recv_value[num_keywords][value_bytes]  = {0};  // That does not mean it is filled with 0's
char value[num_keywords][value_bytes]       = {0};  // The array that holds the verified data
static byte blockindex = 0;
bool new_data = false;
bool blockend = false;

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REDLED, INPUT);
  pinMode(GRNLED, INPUT);
  pinMode(BLULED, INPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(19200);
  //    Serial.begin(19200);


  wifiInit();
  otaInit();
  serverInit();
  delay(500);
  TelnetStream.begin();
  Serial.swap();

  ledOff();
}

void loop() {
  // Receive information on Serial from MPPT
  switch (TelnetStream.read())
  {
    case 'Z':   // Reset ESP
      TelnetStream.stop();
      delay(100);
      ESP.reset();
      break;
    case 'Q':   // Quit Telnet
      TelnetStream.println("bye bye");
      TelnetStream.flush();
      TelnetStream.stop();
      break;
    case 'S':  // Swap Serial
      TelnetStream.println("Swapping Serial");
      Serial.swap();
      break;
  }

  ArduinoOTA.handle();
  RecvWithEndMarker();
  HandleNewData();

  // Just print the values every second,
  // Add your own code here to use the data.
  // Make sure to not used delay(X)s of bigger than 50ms,
  // so make use of the same principle used in PrintEverySecond()
  // or use some sort of Alarm/Timer Library
  PrintEverySecond();
}

// Serial Handling
// ---
// This block handles the serial reception of the data in a
// non blocking way. It checks the Serial line for characters and
// parses them in fields. If a block of data is send, which always ends
// with "Checksum" field, the whole block is checked and if deemed correct
// copied to the 'value' array.

void RecvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (Serial.available() > 0 && new_data == false) {
    rc = Serial.read();
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= buffsize) {
        ndx = buffsize - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      new_data = true;
    }
    yield();
  }
}

void HandleNewData() {
  // We have gotten a field of data
  if (new_data == true) {
    //Copy it to the temp array because parseData will alter it.
    strcpy(tempChars, receivedChars);
    ParseData();
    new_data = false;
  }
}

void ParseData() {
  char * strtokIndx; // this is used by strtok() as an index
  strtokIndx = strtok(tempChars, "\t");     // get the first part - the label
  // The last field of a block is always the Checksum
  if (strcmp(strtokIndx, "Checksum") == 0) {
    blockend = true;
  }
  strcpy(recv_label[blockindex], strtokIndx); // copy it to label

  // Now get the value
  strtokIndx = strtok(NULL, "\r");    // This continues where the previous call left off until '/r'.
  if (strtokIndx != NULL) {           // We need to check here if we don't receive NULL.
    strcpy(recv_value[blockindex], strtokIndx);
  }
  blockindex++;

  if (blockend) {
    // We got a whole block into the received data.
    // Check if the data received is not corrupted.
    // Sum off all received bytes should be 0;
    byte checksum = 0;
    for (int x = 0; x < blockindex; x++) {
      // Loop over the labels and value gotten and add them.
      // Using a byte so the the % 256 is integrated.
      char *v = recv_value[x];
      char *l = recv_label[x];
      while (*v) {
        checksum += *v;
        v++;
      }
      while (*l) {
        checksum += *l;
        l++;
      }
      // Because we strip the new line(10), the carriage return(13) and
      // the horizontal tab(9) we add them here again.
      checksum += 32;
    }
    // Checksum should be 0, so if !0 we have correct data.
    if (!checksum) {
      // Since we are getting blocks that are part of a
      // keyword chain, but are not certain where it starts
      // we look for the corresponding label. This loop has a trick
      // that will start searching for the next label at the start of the last
      // hit, which should optimize it.
      int start = 0;
      for (int i = 0; i < blockindex; i++) {
        for (int j = start; (j - start) < num_keywords; j++) {
          if (strcmp(recv_label[i], keywords[j % num_keywords]) == 0) {
            // found the label, copy it to the value array
            strcpy(value[j], recv_value[i]);
            start = (j + 1) % num_keywords; // start searching the next one at this hit +1
            break;
          }
        }
      }
    }
    // Reset the block index, and make sure we clear blockend.
    blockindex = 0;
    blockend = false;
  }
}

void PrintEverySecond() {
  static unsigned long prev_millis;
  if (millis() - prev_millis > 1000) {
    PrintValues();
    prev_millis = millis();
  }
}


void PrintValues() {
  for (int i = 0; i < num_keywords; i++) {
    TelnetStream.print(keywords[i]);
    TelnetStream.print(",");
    TelnetStream.println(value[i]);
    TelnetStream.println();
  }
//  TelnetStream.print(".");
//  ledSwap();
}



//------------------------------------------------------------------------------
void jsonrequest(AsyncWebServerRequest *request) {
  String JSON = F("[");
  for (int i = 0; i < num_keywords; i++) {
    JSON += "{\"id\":\"" + String(keywords[i]) + "\", \"value\":\"" + String(value[i]) + "\"},";
  }
  JSON += "{\"id\":\"end\", \"value\":\"end\"}";
  JSON += "]";
  request->send(200, "text/json", JSON);
}
//------------------------------------------------------------------------------
void indexrequest(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_page);
}
//------------------------------------------------------------------------------
void ledOn() {
  digitalWrite(LED_BUILTIN, LOW);
}
//------------------------------------------------------------------------------
void ledOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}
//------------------------------------------------------------------------------
void ledSwap() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}
//------------------------------------------------------------------------------
static void wifiInit() {
  WiFi.persistent(false);                                                       // Do not write new connections to FLASH
  WiFi.mode(WIFI_STA);
#if defined ( USE_STATIC_IP )
  WiFi.config(ip, gateway, subnet);                                             // Set fixed IP Address
#endif
  WiFi.begin(wifi_ssid, wifi_password);

  while ( WiFi.status() != WL_CONNECTED ) {                                     //  Wait for WiFi connection
    ledSwap();
    delay(100);
  }
}
//------------------------------------------------------------------------------
void otaInit() {
  ArduinoOTA.setHostname(devicename.c_str());

  ArduinoOTA.onStart([]() {
    ledOn();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    ledSwap();
  });
  ArduinoOTA.onEnd([]() {
    ledOff();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ledOff();
  });
  ArduinoOTA.begin();
}
//------------------------------------------------------------------------------
void serverInit() {
  server.on("/", HTTP_GET, indexrequest);
  server.on("/rest", HTTP_GET, jsonrequest);
  server.onNotFound([](AsyncWebServerRequest * request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(204);
    }
    else {
      request->send(404);
    }
  });
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH");
  server.begin();
}
