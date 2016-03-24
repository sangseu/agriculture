//======================================================Pub_Sub_SmartConfig OK
#include <EEPROM.h>
#include <pfodESP8266Utils.h>
#include <pfodESP8266WiFi.h>
//#include <pfodWiFiClient.h>
#include <pfodESP8266WebServer.h>
#include <pfodESP8266BufferedClient.h>

//PubSub
#include <PubSubClient.h>

//PubSub
#define mqtt_server "128.199.139.40"
#define mqtt_topic_pub "root/agriculture/air/esp/stt"
#define mqtt_topic_pub_lux "root/agriculture/air/esp/lux"
#define mqtt_topic_pub_hum "root/agriculture/air/esp/hum"
#define mqtt_topic_pub_tem "root/agriculture/air/esp/tem"
#define mqtt_topic_sub "root/agriculture/air/esp/control"
const uint16_t mqtt_port = 1883;

// normally DEBUG is commented out
#define DEBUG
#define pubsub
// Initial baud rate for DEBUG is 115200,
// If not starting in web config mode the baud rate is switched to the configured baud.

// uncomment this to connect / disconnect messages on Serial out.
//#define CONNECTION_MESSAGES

// the default to show in the config web page
const int DEFAULT_CONNECTION_TIMEOUT_SEC = 15;
WiFiServer server(80);
WiFiClient client; // just one client reused
pfodESP8266BufferedClient bufferedClient;

// =============== start of pfodWifiWebConfig settings ==============
#define pfodWifiWebConfigPASSWORD "123456789"
#define pfodWifiWebConfigAP "esp_air"

// note pfodSecurity uses 19 bytes of eeprom usually starting from 0 so
// start the eeprom address from 20 for configureWifiConfig
const uint8_t webConfigEEPROMStartAddress = 20;

int toReset = 16;//Pin connect to Reset to hardreset esp
int wifiSetup_pin = 13; // name the input pin for setup mode detection GPIO13 on ESP8266-07 boards
int relay = 4;
// =============== end of pfodWifiWebConfig settings ==============

// On ESP8266-01 and Adafruit HAZZAH ESP8266, connect LED + 270ohm resistor from D2 (GPIO2) to +3V3 to indicate when in config mode

// set the EEPROM structure
struct EEPROM_storage {
  uint32_t timeout;
  uint32_t baudRate;
  uint16_t portNo;
  char ssid[pfodESP8266Utils::MAX_SSID_LEN + 1]; // WIFI ssid + null
  char password[pfodESP8266Utils::MAX_PASSWORD_LEN + 1]; // WiFi password,  if empyt use OPEN, else use AUTO (WEP/WPA/WPA2) + null
  char staticIP[pfodESP8266Utils::MAX_STATICIP_LEN + 1]; // staticIP, if empty use DHCP + null
} storage;
const int EEPROM_storageSize = sizeof(EEPROM_storage);
char ssid_found[pfodESP8266Utils::MAX_SSID_LEN + 1]; // max 32 chars + null

//=======================================
WiFiClient espClient;
PubSubClient clientPS(espClient);
long lastMsg = 0;
char msg[50];

char lux[7];
char hum[7];
char tem[7];

int value = 0;
int l, h, t, e = 0;

bool alreadyConnected = false;
//=======================================
ESP8266WebServer webserver(80);  // this just sets portNo nothing else happens until begin() is called

byte inConfigMode = 0; // false
uint32_t timeout = 0;
const char *aps;

void setup ( void ) {
  pinMode(relay, OUTPUT);
  WiFi.mode(WIFI_STA);
  inConfigMode = 0; // non in config mode
  EEPROM.begin(512); // must be greater than (wifiConfigStartAddress + EEPROM_storageSize)
  delay(10);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW); // make GPIO0 low after ESP8266 has initialized

  for (int i = 4; i > 0; i--) {
    delay(500);
  }
  //esp need Serial to call_UNO
  Serial.begin(115200);
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Starting Setup"));
  //  bufferedClient.setDebugStream(&Serial);  // add this line if using DEBUG in pfodESP8266BufferedClient library code
#endif

  //=========================================15s to put GPIO2-220ohm-GND
  pinMode(wifiSetup_pin, INPUT_PULLUP);
  Serial.println();
  for (int i = 15; i > 0; i--) {
#ifdef DEBUG
    Serial.print(i);
    Serial.print(' ');
#endif
    if (digitalRead(wifiSetup_pin) == LOW) {
      break; // continue to config mode
    }
    // else wait for 20sec to let the user press the button
    delay(1000);
  }
  Serial.println();
  //===================================================================
  //============ pfodWifiConfigV1 config in Access Point mode ====================
  // see if config button is pressed
  //pinMode(wifiSetup_pin, INPUT_PULLUP);
  if (digitalRead(wifiSetup_pin) == LOW) {
    inConfigMode = 1; // in config mode
    WiFi.mode(WIFI_AP_STA);
#ifdef DEBUG
    Serial.println(F("Setting up Access Point for WifiWebConfig"));
#endif
    // connect to temporary wifi network for setup
    // the features determine the format of the {set...} command
    setupAP(pfodWifiWebConfigAP, pfodWifiWebConfigPASSWORD);
    //   Need to reboot afterwards
    return; // skip rest of setup();
  }
  //============================= end pfodWifiConfigV1 config ====================


  // else button was not pressed continue to load the stored network settings
  //else use configured setttings from EEPROM
  uint8_t * byteStorageRead = (uint8_t *)&storage;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    byteStorageRead[i] = EEPROM.read(webConfigEEPROMStartAddress + i);
  }

  timeout = storage.timeout; // set the timeout

  // Initialise wifi module
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Connecting to AP"));
  Serial.print("ssid '");
  Serial.print(storage.ssid);
  Serial.println("'");
  Serial.print("password '");
  Serial.print(storage.password);
  Serial.println("'");
#endif

  //We will go to PubSub=====================
  //Setup Wi-Fi
  WiFi.begin(storage.ssid, storage.password);
  int retry = 0; //20s
  while (WiFi.status() != WL_CONNECTED) {//retry 20s, if cannot connect, esp will as STA
    delay(500);
    if (retry >= 40)
    {
      inConfigMode = 1; // in config mode
      WiFi.mode(WIFI_AP_STA);
#ifdef DEBUG
      Serial.println(F("Setting up Access Point for WifiWebConfig"));
#endif
      // connect to temporary wifi network for setup
      // the features determine the format of the {set...} command
      setupAP(pfodWifiWebConfigAP, pfodWifiWebConfigPASSWORD);
      break;
    }
    retry++;
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Connected!"));
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  //===================================================================Setup PubSub
  clientPS.setServer(mqtt_server, mqtt_port);
  clientPS.setCallback(callback);
}//================================================================================End setup()

//Setup for Server, buff
unsigned long timeoutTimerStart = 0;
const unsigned long SEND_DELAY_TIME = 10; // 10mS delay before sending buffer
unsigned long sendTimerStart = 0;
static const size_t SEND_BUFFER_SIZE = 1460; // Max data size for standard TCP/IP packet
static uint8_t sendBuffer[SEND_BUFFER_SIZE]; //
size_t sendBufferIdx = 0;
//===============================================================================LOOP
// the loop routine runs over and over again forever:
void loop() {
  if (inConfigMode) {
    webserver.handleClient();
    delay(0);
    return;
  }

  //PubSub
  if (!clientPS.connected()) {
    reconnect();
  }
  clientPS.loop();

  long now = millis();
  if (now - lastMsg > 10000)
  {
    lastMsg = now;

    ++value;
    String mess = "";
    Serial.print("y");
    String content = "";
    char character;

    while (Serial.available()) {
      character = Serial.read();
      content.concat(character);
    }
    if (content != "")
    {
      Serial.print(content);
      Serial.print("length");
      Serial.println(content.length());

      //abcl123h456t789s50@
      //lxxxhxxxtxxx@
      for (int i = 0; i < content.length(); i++)
      {
        l = content.indexOf('l');
        h = content.indexOf('h');
        t = content.indexOf('t');
        e = content.indexOf('$');
      }
      if (l < h < t < e) {
        Serial.println(l);
        Serial.println(h);
        Serial.println(t);
        Serial.println(e);
        /*change lux[5] to lux[7]
        for (int i = 0; i < (h - l - 1); i ++) {
          lux[i] = content[l + 1 + i];
        }
        for (int i = 0; i < (t - h - 1); i ++) {
          hum[i] = content[h + 1 + i];
        }
        for (int i = 0; i < (e - t - 1); i ++) {
          tem[i] = content[t + 1 + i];
        }
        */
        for (int i = 0; i < (h - l); i ++) {
          lux[i] = content[l + i];
        }
        for (int i = 0; i < (t - h); i ++) {
          hum[i] = content[h + i];
        }
        for (int i = 0; i < (e - t); i ++) {
          tem[i] = content[t + i];
        }
        Serial.println(lux);
        Serial.println(hum);
        Serial.println(tem);

        clientPS.publish(mqtt_topic_pub_lux, lux);
        clientPS.publish(mqtt_topic_pub_hum, hum);
        clientPS.publish(mqtt_topic_pub_tem, tem);
        for (int i = 0; i < 7; i++)
        {
          lux[i] = '\0';
          hum[i] = '\0';
          tem[i] = '\0';
        }//end for
      }//end if
    }//end if
  }//end if
  /*
  // see if we should drop the connection
  if (alreadyConnected && (timeout > 0) && ((millis() - timeoutTimerStart) > timeout)) {
    closeConnection();
  }
  */
}//===============================================================================end LOOP

void closeConnection() {
#ifdef CONNECTION_MESSAGES
  Serial.println("ConnectionClosed");
#endif
  alreadyConnected = false;
  bufferedClient.stop(); // clears client reference
  client.stop(); // stop client and tcp buffer.
  if (server.hasClient()) {   // avoid creating WiFiClient object if no connection,  ESP8266 specific
    client = server.available();
  } // else just keep client that was just stopped will evaluate to false
}

void setupAP(const char* ssid_wifi, const char* password_wifi) {
  aps = pfodESP8266Utils::scanForStrongestAP((char*)&ssid_found, pfodESP8266Utils::MAX_SSID_LEN + 1);
  delay(0);
  IPAddress local_ip = IPAddress(10, 1, 1, 1);
  IPAddress gateway_ip = IPAddress(10, 1, 1, 1);
  IPAddress subnet_ip = IPAddress(255, 255, 255, 0);
#ifdef DEBUG
  Serial.println(F("configure WifiWebConfig"));
#endif
  WiFi.softAP(ssid_wifi, password_wifi);

#ifdef DEBUG
  Serial.println(F("Access Point setup"));
#endif
  WiFi.softAPConfig(local_ip, gateway_ip, subnet_ip);

#ifdef DEBUG
  Serial.println("done");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(myIP);
#endif
  delay(10);
  webserver.on ( "/", handleRoot );
  webserver.on ( "/config", handleConfig );
  webserver.onNotFound ( handleNotFound );
  webserver.begin();
#ifdef DEBUG
  Serial.println ( "HTTP webserver started" );
#endif
}

void handleConfig() {
  // set defaults
  uint16_t portNo = 80;
  uint32_t timeout = DEFAULT_CONNECTION_TIMEOUT_SEC * 1000; // time out in 15 sec
  uint32_t baudRate = 19200;

  if (webserver.args() > 0) {
#ifdef DEBUG
    String message = "Config results\n\n";
    message += "URI: ";
    message += webserver.uri();
    message += "\nMethod: ";
    message += ( webserver.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += webserver.args();
    message += "\n";
    for ( uint8_t i = 0; i < webserver.args(); i++ ) {
      message += " " + webserver.argName ( i ) + ": " + webserver.arg ( i ) + "\n";
    }
    Serial.println(message);
#endif

    uint8_t numOfArgs = webserver.args();
    const char *strPtr;
    uint8_t i = 0;
    for (; (i < numOfArgs); i++ ) {
      // check field numbers
      if (webserver.argName(i)[0] == '1') {
        pfodESP8266Utils::strncpy_safe(storage.ssid, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_SSID_LEN);
        pfodESP8266Utils::urldecode2(storage.ssid, storage.ssid); // result is always <= source so just copy over
      } else if (webserver.argName(i)[0] == '2') {
        pfodESP8266Utils::strncpy_safe(storage.password, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_PASSWORD_LEN);
        pfodESP8266Utils::urldecode2(storage.password, storage.password); // result is always <= source so just copy over
        // if password all blanks make it empty
        if (pfodESP8266Utils::isEmpty(storage.password)) {
          storage.password[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '3') {
        pfodESP8266Utils::strncpy_safe(storage.staticIP, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_STATICIP_LEN);
        pfodESP8266Utils::urldecode2(storage.staticIP, storage.staticIP); // result is always <= source so just copy over
        if (pfodESP8266Utils::isEmpty(storage.staticIP)) {
          // use dhcp
          storage.staticIP[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '4') {
        // convert portNo to uint16_6
        const char *portNoStr = (( webserver.arg(i)).c_str());
        long longPort = 0;
        pfodESP8266Utils::parseLong((byte*)portNoStr, &longPort);
        storage.portNo = (uint16_t)longPort;
      } else if (webserver.argName(i)[0] == '5') {
        // convert baud rate to int32_t
        const char *baudStr = (( webserver.arg(i)).c_str());
        long baud = 0;
        pfodESP8266Utils::parseLong((byte*)baudStr, &baud);
        storage.baudRate = (uint32_t)(baud);
      } else if (webserver.argName(i)[0] == '6') {
        // convert timeout to int32_t
        // then *1000 to make mS and store as uint32_t
        const char *timeoutStr = (( webserver.arg(i)).c_str());
        long timeoutSec = 0;
        pfodESP8266Utils::parseLong((byte*)timeoutStr, &timeoutSec);
        if (timeoutSec > 4294967) {
          timeoutSec = 4294967;
        }
        if (timeoutSec < 0) {
          timeoutSec = 0;
        }
        storage.timeout = (uint32_t)(timeoutSec * 1000);
      }
    }

    uint8_t * byteStorage = (uint8_t *)&storage;
    for (size_t i = 0; i < EEPROM_storageSize; i++) {
      EEPROM.write(webConfigEEPROMStartAddress + i, byteStorage[i]);
    }
    delay(0);
    EEPROM.commit();
  } // else if no args just return current settings

  delay(0);
  struct EEPROM_storage storageRead;
  uint8_t * byteStorageRead = (uint8_t *)&storageRead;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    byteStorageRead[i] = EEPROM.read(webConfigEEPROMStartAddress + i);
  }

  String rtnMsg = "<html>"
                  "<head>"
                  "<title>pfodWifiWebConfig Server Setup</title>"
                  "<meta charset=\"utf-8\" />"
                  "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
                  "</head>"
                  "<body>"
                  "<h2>pfodWifiWebConfig Server Settings saved.</h2><br>Power cycle to connect to ";
  if (storageRead.password[0] == '\0') {
    rtnMsg += "the open network ";
  }
  rtnMsg += "<b>";
  rtnMsg += storageRead.ssid;
  rtnMsg += "</b>";

  if (storageRead.staticIP[0] == '\0') {
    rtnMsg += "<br> using DCHP to get its IP address";
  } else { // staticIP
    rtnMsg += "<br> using IP addess ";
    rtnMsg += "<b>";
    rtnMsg += storageRead.staticIP;
    rtnMsg += "</b>";
  }
  rtnMsg += "<br><br>Will start a server listening on port ";
  rtnMsg += storageRead.portNo;
  rtnMsg += ".<br> with connection timeout of ";
  rtnMsg += storageRead.timeout / 1000;
  rtnMsg += " seconds.";
  rtnMsg += "<br><br>Serial baud rate set to ";
  rtnMsg += storageRead.baudRate;
  "</body>"
  "</html>";

  webserver.send ( 200, "text/html", rtnMsg );
  pinMode(toReset, OUTPUT);//pin16 to reset
  //=======================================================================reset esp
}


void handleRoot() {
  String msg;
  msg = "<html>"
        "<head>"
        "<title>pfodWifiWebConfig Server Setup</title>"
        "<meta charset=\"utf-8\" />"
        "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
        "</head>"
        "<body>"
        "<h2>pfodWifiWebConfig Server Setup</h2>"
        "<p>Use this form to configure your device to connect to your Wifi network and start as a Server listening on the specified port.</p>"
        "<form class=\"form\" method=\"post\" action=\"/config\" >"
        "<p class=\"name\">"
        "<label for=\"name\">Network SSID</label><br>"
        "<input type=\"text\" name=\"1\" id=\"ssid\" placeholder=\"wifi network name\"  required "; // field 1

  if (*aps != '\0') {
    msg += " value=\"";
    msg += aps;
    msg += "\" ";
  }
  msg += " />"
         "<p class=\"password\">"
         "<label for=\"password\">Password for WEP/WPA/WPA2 (enter a space if there is no password, i.e. OPEN)</label><br>"
         "<input type=\"text\" name=\"2\" id=\"password\" placeholder=\"wifi network password\" autocomplete=\"off\" required " // field 2
         "</p>"
         "<p class=\"static_ip\">"
         "<label for=\"static_ip\">Set the Static IP for this device</label><br>"
         "(If this field is empty, DHCP will be used to get an IP address)<br>"
         "<input type=\"text\" name=\"3\" id=\"static_ip\" placeholder=\"192.168.4.99\" "  // field 3
         " pattern=\"\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b\"/>"
         "</p>"
         "<p class=\"portNo\">"
         "<label for=\"portNo\">Set the port number that the Server will listen on for connections.</label><br>"
         "<input type=\"text\" name=\"4\" id=\"portNo\" placeholder=\"80\" required"  // field 4
         " pattern=\"\\b([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])\\b\" />"
         "</p>"
         "<p class=\"baud\">"
         "<label for=\"baud\">Serial Baud Rate (limit to 19200 for Uno and Mega)</label><br>"
         "<select name=\"5\" id=\"baud\" required>" // field 5
         "<option value=\"9600\">9600</option>"
         "<option value=\"14400\">14400</option>"
         "<option selected value=\"19200\">19200</option>"
         "<option value=\"28800\">28800</option>"
         "<option value=\"38400\">38400</option>"
         "<option value=\"57600\">57600</option>"
         "<option value=\"76800\">76800</option>"
         "<option value=\"115200\">115200</option>"
         "</select>"
         "</p>"
         "<p class=\"timeout\">"
         "<label for=\"timeout\">Set the server connection timeout in seconds, 0 for never timeout (not recommended).</label><br>"
         "<input type=\"text\" name=\"6\" id=\"timeout\" required"  // field 6
         " value=\"";
  msg +=   DEFAULT_CONNECTION_TIMEOUT_SEC;
  msg +=  "\""
          " pattern=\"\\b([0-9]{1,7})\\b\" />"
          "</p>"
          "<p class=\"submit\">"
          "<input type=\"submit\" value=\"Configure\"  />"
          "</p>"
          "</form>"
          "</body>"
          "</html>";

  webserver.send ( 200, "text/html", msg );
}


void handleNotFound() {
  handleRoot();
}

//PubSub
void reconnect() {
  // Loop until we're reconnected
  while (!clientPS.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    //if (clientPS.connect("ESP8266Client", mqtt_user, mqtt_pwd)) {
    if (clientPS.connect("ESP8266Client_01")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      clientPS.publish(mqtt_topic_pub, "ESP_reconnected");
      // ... and resubscribe
      clientPS.subscribe(mqtt_topic_sub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(clientPS.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }//end while
}//end function

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

void callback(char* topic, byte* payload, unsigned int length)
{

  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  pinMode(relay, OUTPUT);
  if ((char)payload[0] == '1') {
    digitalWrite(relay, HIGH);   // Turn the relay on (Note that LOW is the voltage level
    Serial.println("relay");
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(relay, LOW);  // Turn the relay off by making the voltage HIGH
    Serial.println("relay_no");
  }
}





