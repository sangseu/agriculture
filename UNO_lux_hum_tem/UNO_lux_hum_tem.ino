/*
BH1750FVI Light senosr
  VCC >>> 3.3V
  SDA >>> A4
  SCL >>> A5
  addr >> A3//no connect
  Gnd >>>Gnd
DHT sensor
  data pin A2
*/

// First define the library :

#include <Wire.h>
#include <BH1750.h>
#include "DHT.h"

#define DHTPIN A2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
long last_time = 0;

uint16_t lux = 0;
uint16_t humidity = 0;
uint16_t temperature = 0;

void setup()
{
  Serial.begin(115200);
  //init BH1750
  lightMeter.begin();
  Serial.println("Running...");
  pinMode(13, OUTPUT);
}

void show_value()
{
  Serial.print("Lux: ");
  Serial.print(lux);
  Serial.print("\t\t");
  Serial.print("Hum: ");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print("Tem: ");
  Serial.print(temperature, 1);
  Serial.println();
}

void to_esp()
{
  String data = "";
  data = data + "l" + String(lux) + "@" + "h" + String(humidity) + "@" + "t" + String(temperature) + "@" + "$";
  Serial.print(data);
  /*
  Serial.print('l');
  Serial.print(lux);
  Serial.print('h');
  Serial.print(humidity, 1);
  Serial.print('t');
  Serial.print(temperature, 1);
  Serial.print('@');
  */
}
/*
void serialEvent(){
  show_value();
}
*/

void loop() {
  // Get Lux value
  lux = lightMeter.readLightLevel();
  // Get DHT value
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  //String DHTstt = dht.getStatusString();

  if (Serial.available() > 0)
  {
    if ( Serial.read() == 'y')
    {
      to_esp();
      //lxxxxhxxxxtxxxx@
      digitalWrite(13, LOW);
      delay(50);
      digitalWrite(13, HIGH);
      delay(50);
    }
  }

}//===================================================================================END LOOP

/*
  long now = millis();
  if (now - last_time > 2000)
  {
    last_time = now;
    show_value();
  }//end if
  */


