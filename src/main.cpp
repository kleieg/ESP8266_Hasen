#include <Arduino.h>
/*


Selections:
Board: NodeMCU 1.0 (ESP12E Module)
Uploadspeed: 921600
CPU Frequency: 80MHz
Flash Size: 4MB

Port: /dev/c.wchusbserial1440

siehe auch Steckerleiste!!!
   
 */

// Import required libraries
#include "ESP8266WiFi.h"
#include "ESPAsyncWebServer.h"

#include "WLAN_Credentials.h"


// Set number of relays
#define NUM_RELAYS  6

// Assign each GPIO to a relay
int relayGPIOs[NUM_RELAYS] = {16, 5, 4, 14, 12, 13};
String relayName[NUM_RELAYS] = {"Klappe 1", "Klappe 2", "Klappe 3", "Klappe 4", "Klappe 5", "Klappe 6"};
String relayReset[NUM_RELAYS] = {"Y", "Y", "Y", "Y", "Y", "Y"};
String relayState[NUM_RELAYS] = {"", "", "", "", "", ""};
int relayResetStatus[NUM_RELAYS] = {0,0,0,0,0,0};
int relayResetTimer[NUM_RELAYS] = {0,0,0,0,0,0};

// Timers auxiliar variables
long now = millis();
int LEDblink = 0;
bool led = 1;
int gpioLed = 2;
int RelayResetTime = 300;


const char* PARAM_INPUT_1 = "relay";  
const char* PARAM_INPUT_2 = "state";


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 48px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 32px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Hasen Server</h2>
  %BUTTONPLACEHOLDER%
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?relay="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?relay="+element.id+"&state=0", true); }
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    for(int i=1; i<=NUM_RELAYS; i++){
      buttons+= "<h4>Klappe #" + String(i) + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" "+ relayState[i-1] +"><span class=\"slider\"></span></label>";
    }
    return buttons;
  }
  return String();
}


void setup(){
  
  Serial.begin(115200);

  pinMode(gpioLed, OUTPUT);
  digitalWrite(gpioLed,led);

  // Set all relays to off when the program starts -  the relay is off when you set the relay to HIGH
  for(int i=1; i<=NUM_RELAYS; i++){
    pinMode(relayGPIOs[i-1], OUTPUT);
    digitalWrite(relayGPIOs[i-1], HIGH);
  }
  
  // Connect to Wi-Fi
  Serial.println("");
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.hostname("Hasen");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // Print ESP8266 Local IP Address
  Serial.println("");
  Serial.print("Connected to IP: ");
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/update?relay=<inputMessage>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    String inputMessage2;
    String inputParam2;
    // GET input1 value on <ESP_IP>/update?relay=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1) & request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      inputParam2 = PARAM_INPUT_2;

      digitalWrite(relayGPIOs[inputMessage.toInt()-1], !inputMessage2.toInt());
      if (inputMessage2.toInt() == 1) {
        relayState[inputMessage.toInt()-1] = "checked";
      }
      else {
        relayState[inputMessage.toInt()-1] = "";
      }

      if (relayReset[inputMessage.toInt()-1] == "Y") {
        relayResetStatus[inputMessage.toInt()-1] = 1;
      }
 
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println("Klappe " + inputMessage + "   Status " + inputMessage2);
    request->send(200, "text/plain", "OK");
  });
  // Start server
  server.begin();
}
  
void loop() {
// LED blinken
    now = millis();

    if (now - LEDblink > 2000) {
      LEDblink = now;
      if(led == 0) {
       digitalWrite(gpioLed, 1);
       led = 1;
      }else{
       digitalWrite(gpioLed, 0);
       led = 0;
      }
    }

// auf Reset prüfen
// falls nötig Timer setzten
  for(int i=1; i<=NUM_RELAYS; i++){
    if (relayResetStatus[i-1] == 1) {
      relayResetStatus[i-1] = 2;
      relayResetTimer[i-1] = now;
    }
  }

// prüfen ob Timer abgelaufen; wenn ja Relais ausschalten
  for(int i=1; i<=NUM_RELAYS; i++){
    if (relayResetStatus[i-1] == 2 ){
      if (now - relayResetTimer[i-1] > RelayResetTime ){
        relayResetStatus[i-1] = 0;
        digitalWrite(relayGPIOs[i-1], HIGH);
      }
    }
  }  
}