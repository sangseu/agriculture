
/*
  This is a simple code to test BH1750FVI Light senosr
  communicate using I2C Protocol 
  this library enable 2 slave device address
  Main address  0x23 
  secondary address 0x5C 
  connect this sensor as following :
  VCC >>> 3.3V
  SDA >>> A4 
  SCL >>> A5
  addr >> A3
  Gnd >>>Gnd

  Written By : Mohannad Rawashdeh
  
 */
 
 // First define the library :

#include <Wire.h>
#include <BH1750FVI.h>
#include "DHT.h"

DHT dht;

BH1750FVI LightSensor;


void setup() {   // put your setup code here, to run once:
   Serial.begin(115200);
  dht.setup(A2); // data pin A2
  LightSensor.begin();
  /*
 Set the address for this sensor 
 you can use 2 different address
 Device_Address_H "0x5C"
 Device_Address_L "0x23"
 you must connect Addr pin to A3 .
 */
  LightSensor.SetAddress(Device_Address_H);//Address 0x5C
 // To adjust the slave on other address , uncomment this line
 // lightMeter.SetAddress(Device_Address_L); //Address 0x5C
 //-----------------------------------------------
  /*
   set the Working Mode for this sensor 
   Select the following Mode:
    Continuous_H_resolution_Mode
    Continuous_H_resolution_Mode2
    Continuous_L_resolution_Mode
    OneTime_H_resolution_Mode
    OneTime_H_resolution_Mode2
    OneTime_L_resolution_Mode
    
    The data sheet recommanded To use Continuous_H_resolution_Mode
  */

  LightSensor.SetMode(Continuous_H_resolution_Mode);
  
  Serial.println("Running...");
}


void loop() {
  // put your main code here, to run repeatedly: 
  uint16_t lux = LightSensor.GetLightIntensity();// Get Lux value
  
  //delay(1000);

  delay(dht.getMinimumSamplingPeriod());

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  
  

long now = millis();
if(now - last_time > 2000)
{
  last_time = now;

  //lux
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lux");
  //hum-tem
  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print("Hum: ");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print("Tem: ");
  Serial.print(temperature, 1);
  Serial.println();
}//end if
}
