/* Victron connector ***
         _______________
        |               |
        |  Victron JST  |
        |_______________|
        |YEL|WHT|RED|BLK|
        | 1 | 2 | 3 | 4 |
        |+5V|TX |RX |GND|
        |___|_:::::_|___|
              |
              |
              V . Connect directly to D7 / GPIO13
              (don't mind the 5V level; the output is very weak and will be clipped to 3,3V anyway)

*** Witty ESP8266 or Wemos D1 Pinout

          REST         |       TX0
          ADC    LDR   |       RX0
          CH_PD        |       GPIO05  D1
  D0      GPIO16       | BTN   GPIO04  D2
  D5      GPIO14       |       GPIO00  D3
  D6      GPIO12 RGB-G |       GPIO02  D4 TX1
  D7 RX0* GPIO13 RGB-B | RGB-R GPIO15  D8 TX0*
          VCC          |       GND

   after Serial.swap()

  Feed the Witty ESP8266 or Wemos D1 over a 12V / 5V micro buck converter

  Get values via WebUI
  http://<IPADDR>

  Get values via JSON
  GET http://<IPADDR>/rest

  Get values via Telnet
  telnet <IPADDR>
    once connected:

  Z resets the ESP
  Q Quits telnet
  S Swaps Serial to default/to D7+D8 (Toggle, restores Serial monitor but break Victron communication)


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


//***Operating Values from Victron***
float BatV;           // V   Battery voltage, V
float BatI;           // I   Battery current, A
float PanV;           // VPV Panel voltage,   V
float PanW;           // PPV Panel power,     W
int   ChSt;           // CS  Charge state, 0 to 9
int   Err;            // ERR Error code, 0 to 119
boolean LodOn ;       // LOAD ON Load output state, ON/OFF
float LodI ;          // IL  Load Current    ,A
float TotKWh ;        // H19 Yield total, kWh
float TodKWh ;        // H19 Yield today, kWh
float TodMP ;         // H21 397 Maximum power today, W
float YesKWh ;        // H22 Yield yesterday, kWh
float YesMP ;         // H23 Maximum power yesterday, W
int   DaysOn ;        // HSDS Day sequence number, 0 to 365

//***Operating Values derived***
float BatW ;          //  BatV*BatI
float PanI ;          //  PanW/PanV
float LodW ;          //  LodI*BatV


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
    GatherValues();
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

void PrintEverySecond()
{
  static unsigned long prev_millis;
  if (millis() - prev_millis > 1000) 
  {
    PrintValues();
    prev_millis = millis();
  }
}

void PrintValues()
{
/*  for (int i = 0; i < num_keywords; i++) 
  {
    TelnetStream.print(keywords[i]);
    TelnetStream.print(",");
    TelnetStream.println(value[i]);
  }
*/
  TelnetStream.printf("BatV:%3.2f BatI:%3.2f PanV: %3.2f PanW: %3.2f, LoadI %3.2f\n", BatV, BatI, PanV, PanW, LodI);
  //  ledSwap();
}

void GatherValues()
{
  for (int i = 0; i < num_keywords; i++) {
    if (String(keywords[i]) == "V")     BatV = atof(value[i]) / 1000;
    if (String(keywords[i]) == "I")     BatI = atof(value[i]) / 1000;
    if (String(keywords[i]) == "VPV")   PanV = atof(value[i]) / 1000;
    if (String(keywords[i]) == "PPV")   PanW = atof(value[i]) / 1000;
    if (String(keywords[i]) == "IL")    LodI = atof(value[i]) / 1000;
    if (String(keywords[i]) == "CS")    ChSt = atoi(value[i]);
    if (String(keywords[i]) == "ERR")   Err = atoi(value[i]);
  }
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
