/*

Example of BH1750 library usage.

This example initalises the BH1750 object using the default
high resolution mode and then makes a light level reading every second.

Connection:
 VCC-5v
 GND-GND
 SCL-SCL(analog pin 5)
 SDA-SDA(analog pin 4)
 ADD-NC or GND

*/
#include <SHT1x.h>

#include <Wire.h>
#include <BH1750.h>
#define dataPin  14
#define clockPin 12
#define SDA 12
#define SCL 13

BH1750 lightMeter;
SHT1x sht1x(dataPin, clockPin);

void setup(){
  Serial.begin(115200);
  Serial.println("strarting...");
  Serial.println("Running...");
}


void loop() {
  float f_soi, f_tem;
  SHT1x sht1x(dataPin, clockPin);
    f_soi = sht1x.readHumidity();
    f_tem = sht1x.readTemperatureC();
    Wire.begin(SDA,SCL); //Begin I2B for lux sensor
    lightMeter.begin();

  uint16_t f_lux = lightMeter.readLightLevel();
  Serial.println(f_lux);
  Serial.println(f_soi);
  Serial.println(f_tem);
  delay(3000);
}
