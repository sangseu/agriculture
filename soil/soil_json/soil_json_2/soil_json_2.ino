//======================================================Pub_Sub_SmartConfig OK
#include <stdio.h>
#include <EEPROM.h>
#include <pfodESP8266Utils.h>
#include <pfodESP8266WiFi.h>
//#include <pfodWiFiClient.h>
#include <pfodESP8266WebServer.h>
#include <pfodESP8266BufferedClient.h>
//PubSub
#include <PubSubClient.h>
#include <aJSON.h>

#include <SHT1x.h>
//=============================================
#define dataPin  14
#define clockPin 12
#define SCL 12
#define SDA 13
#define relay1 4
#define relay2 5
#define wifiSetup_pin 16
//=============================================
SHT1x sht1x(dataPin, clockPin);

//=============================================
//PubSub
#define mqtt_server "128.199.139.40"
const uint16_t mqtt_port = 1883;

#define mqtt_topic_pub_soi "viaot/esp00000001/out"
#define mqtt_topic_sub "viaot/esp00000001/in"
#define mqtt_topic_pub "viaot/esp00000001/out"
#define mqtt_topic_pub_status "viaot/esp00000001/status"
#define ID_esp "00000001"
//=============================================
//normally DEBUG is commented out
#define DEBUG
#define pubsub

// uncomment this to connect / disconnect messages on Serial out.
//#define CONNECTION_MESSAGES

// the default to show in the config web page
const int DEFAULT_CONNECTION_TIMEOUT_SEC = 15;
WiFiServer server(80);
WiFiClient client; // just one client reused
pfodESP8266BufferedClient bufferedClient;

// =============== start of pfodWifiWebConfig settings ==============
#define pfodWifiWebConfigPASSWORD "123456789"
#define pfodWifiWebConfigAP "esp_00000001"

// note pfodSecurity uses 19 bytes of eeprom usually starting from 0 so
// start the eeprom address from 20 for configureWifiConfig
const uint8_t webConfigEEPROMStartAddress = 20;

// =============== end of pfodWifiWebConfig settings ==============
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
bool alreadyConnected = false;
float f_soi, f_tem;

int value = 0;
int s = 0;

byte inConfigMode = 0; // false
uint32_t timeout = 0;
const char *aps;
//=======================================
ESP8266WebServer webserver(80);  // this just sets portNo nothing else happens until begin() is called

void setup ( void ) {
  WiFi.mode(WIFI_STA);
  inConfigMode = 0; // non in config mode
  EEPROM.begin(512); // must be greater than (wifiConfigStartAddress + EEPROM_storageSize)
  delay(10);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW); // make GPIO0 low after ESP8266 has initialized

  for (int i = 4; i > 0; i--) {
    delay(500);
  }
  //esp need Serial to debug
  Serial.begin(115200);
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Starting Setup"));
  //  bufferedClient.setDebugStream(&Serial);  // add this line if using DEBUG in pfodESP8266BufferedClient library code
#endif

  //=========================================20s to put GPIO16-220ohm-GND
  pinMode(wifiSetup_pin, INPUT_PULLUP);
  Serial.println();

  for (int i = 20; i > 0; i--) {
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
  //=====================================================================
  // see if config button is pressed
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

  //else button was not pressed continue to load the stored network settings
  //use configured setttings from EEPROM
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
  //======================================Setup PubSub
  clientPS.setServer(mqtt_server, mqtt_port);
  clientPS.setCallback(callback);
}//==============================================================================End setup()

//Setup for Server, buff
unsigned long timeoutTimerStart = 0;
const unsigned long SEND_DELAY_TIME = 10; // 10mS delay before sending buffer
unsigned long sendTimerStart = 0;
static const size_t SEND_BUFFER_SIZE = 1460; // Max data size for standard TCP/IP packet
static uint8_t sendBuffer[SEND_BUFFER_SIZE]; //
size_t sendBufferIdx = 0;
//====================================================================================LOOP
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

    f_soi = sht1x.readHumidity();
    f_tem = sht1x.readTemperatureC();

    if (1 < (int)f_soi && (int)f_soi < 99 && 1 < (int)f_tem && (int)f_tem < 99)
    {
      Serial.print(f_soi);
      Serial.print("-");
      Serial.print(f_tem);

      aJsonObject *root, *data;
      root = aJson.createObject();
      aJson.addItemToObject(root, "data", data = aJson.createObject());
      aJson.addNumberToObject(data, "hum", f_soi);
      aJson.addNumberToObject(data, "tem", f_tem);
      clientPS.publish(mqtt_topic_pub_soi, aJson.print(root));
      aJson.deleteItem(root);

      Serial.println(" pub");

    }
    else Serial.println("Cannot found sensor!");
  }//end if
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
  delay(3000);
  ESP.reset();
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
    if (clientPS.connect("ESP8266Client")) {
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
  char json[length + 1];
  for (int i = 0; i < length; i++) {
    //Serial.print((char)payload[i]);
    json[i] = (char)payload[i];
  }
  Serial.println();
  Serial.println(json);
  String s_json = (String)json;

  char* c_json = &s_json[0];
  int ID_relay, wrt_relay;
  aJsonObject* root = aJson.parse(c_json);
  if (root != NULL) {
    aJsonObject* command = aJson.getObjectItem(root, "command");
    if (command != NULL) {
      aJsonObject* gpio = aJson.getObjectItem(command, "gpio");
      if (gpio != NULL) {
        aJsonObject* ID = aJson.getObjectItem(gpio, "ID");
        aJsonObject* wrt = aJson.getObjectItem(gpio, "write");
        if (ID != NULL && wrt != NULL) {
          ID_relay = ID->valueint;
          wrt_relay = wrt->valueint;
        }
      }
    }
  }

  // Switch on the LED if an 1 was received as first character
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);

  if (ID_relay == 0)
  {
    digitalWrite(relay1, wrt_relay);
    Serial.print(ID_relay);
    Serial.println(wrt_relay);
  }
  if (ID_relay == 1)
  {
    digitalWrite(relay2, wrt_relay);
    Serial.print(ID_relay);
    Serial.println(wrt_relay);
  }
}





