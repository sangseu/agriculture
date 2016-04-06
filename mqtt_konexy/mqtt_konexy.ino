/*Chương trình đơn giản demo MQTT Client kết nối tới Broker
 * Client kết nối tới Broker với ID: "ESP8266Client"
 * Topic public: "/log/JBazbe6N"
 * Public bản tin theo form Json: {"mac":"asduyaidha9dh"}
 * Sử dụng thư viện: https://github.com/sangseu/agriculture/tree/master/LIBRARIES/PubSubClient
 * sangseu
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

bool stt = true;

// Update these with values suitable for your network.
const char* ssid = "___________";
const char* password = "___________";
//===================================================
const char* mqtt_server = "103.52.92.8";//iot.konexy.com
unsigned long mqtt_port = 1883;
#define usr "quannv"
#define pwd "123456789"
#define topic_pub "/log/JBazbe6N"

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", usr, pwd)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_pub, "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() - lastMsg > 50) {
    lastMsg = millis();;

    Serial.println((String)millis());
    String msg = "{\"mac\":\"" + (String)millis() + "\"}";
    char* c_msg = &msg[0];
    client.publish(topic_pub, c_msg); //{"mac":"asduyaidha9dh"}
  }
}// end loop
