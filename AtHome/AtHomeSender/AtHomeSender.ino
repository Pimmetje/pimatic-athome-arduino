#include <JeeLib.h>
#include <AtHome.h>
#include <OneWire.h>
#include <DallasTemperature.h> 

//----------------------------------------------------
// Comment in to send serial debug output
//----------------------------------------------------
//#define SERIAL_DEBUG  1
 
  
//----------------------------------------------------
// PINs
//----------------------------------------------------
#define PIN_US_ECHO        3
#define PIN_US_TRIGGER     4
#define PIN_ONE_WIRE       5

//----------------------------------------------------
// Ids
//----------------------------------------------------
#define ID_VOLTAGE             1
#define ID_WATERLVOLUME        2
#define ID_TEMPERATURE_START  10

//----------------------------------------------------
// Ultra sonic sensor
//----------------------------------------------------
#define US_SENSOR_DISTANCE_FROM_GROUND  128.0
#define WATER_SURFACE                   3.1415 * 1.24 * 1.24  // constant for calculating the water volume

//----------------------------------------------------
// OneWire
//----------------------------------------------------
#define TEMPERATURE_PRECISION 9

OneWire oneWire(PIN_ONE_WIRE);
DallasTemperature dtSensors(&oneWire);

//----------------------------------------------------
// interrupt handler for JeeLabs Sleepy power saving
//----------------------------------------------------
ISR(WDT_vect) { Sleepy::watchdogEvent(); } 


 
AtHomeSensorValue valueVoltage;
AtHomeSensorValue valueWaterVolume;
AtHomeSensorValue valueTemperature;
 
 
//--------------------------------------------------------------------------------------------------
// setup
//--------------------------------------------------------------------------------------------------
void setup() 
{  
#if SERIAL_DEBUG  
  Serial.begin(57600);
#endif

  // setup pins
  pinMode(PIN_US_TRIGGER, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  
  // setup values
  valueVoltage.identifier      = ID_VOLTAGE;
  valueVoltage.type            = ValueType_Voltage;
  
  valueWaterVolume.identifier  = ID_WATERLVOLUME;
  valueWaterVolume.type        = ValueType_WaterVolume;
  
  valueTemperature.type        = ValueType_Temperature;
  
  // setup rf
  rf12_initialize(AtHomeJNMasterNodeID,AtHomeJNFreq,AtHomeJNNetworkGroup); // Initialize RFM12 with settings defined above
  rf12_control(0xC000);                                   // Adjust low battery voltage to 2.2V
  rf12_sleep(0);                                          // Put the RFM12 to sleep
 
 // setup jeenode
  PRR = bit(PRTIM1);     // only keep timer 0 going
  ADCSRA &= ~ bit(ADEN); // Disable the ADC
  bitSet (PRR, PRADC);   // Power down ADC
  bitClear (ACSR, ACIE); // Disable comparitor interrupts
  bitClear (ACSR, ACD);  // Power down analogue comparitor
  
  // setup onewire
  dtSensors.begin();

  uint8_t owDeviceCount = dtSensors.getDeviceCount();
#if SERIAL_DEBUG
  Serial.print("Number of OneWire devices found: ");
  Serial.println(owDeviceCount, DEC);
#endif
  
  DeviceAddress owDeviceAddress;
  for (uint8_t i = 0; i < owDeviceCount; i++)
  {
    dtSensors.getAddress(owDeviceAddress, i);
    // set the resolution to 9 bit 
    dtSensors.setResolution(owDeviceAddress, TEMPERATURE_PRECISION);

#if SERIAL_DEBUG
    printOneWireDeviceAddress(owDeviceAddress);
    Serial.println();
#endif
  }

}
 
//--------------------------------------------------------------------------------------------------
// loop
//--------------------------------------------------------------------------------------------------
void loop() 
{
  //----------------------------------------------------
  // water volume
  //----------------------------------------------------
  // trigger ultrasonic sensor to get distance from sensor to water
  delay(100);
  digitalWrite(PIN_US_TRIGGER, LOW);
  delay(2);
  digitalWrite(PIN_US_TRIGGER, HIGH);
  delay(10);
  digitalWrite(PIN_US_TRIGGER, LOW);

  // compute water volume
  float waterHeight = US_SENSOR_DISTANCE_FROM_GROUND - (pulseIn(PIN_US_ECHO, HIGH)/58.0); // water height in cm
  if ( waterHeight < 0 )
    waterHeight = 0;
  int iVolume = (int)(WATER_SURFACE * waterHeight * 10);
  //TODO: digitally filter the water height

  // send water volume
  valueWaterVolume.value       = iVolume;
  rfwrite(&valueWaterVolume, sizeof(valueWaterVolume)); // Send data via RF
  
#if SERIAL_DEBUG
  Serial.print("Water volume (l): ");
  Serial.println(iVolume, DEC);
  delay(1000);
#endif
  
  //----------------------------------------------------
  // OneWire temperature sensors
  //----------------------------------------------------
  dtSensors.requestTemperatures();
  
  uint8_t owDeviceCount = dtSensors.getDeviceCount();
  DeviceAddress owDeviceAddress;
  for (uint8_t i = 0; i < owDeviceCount; i++)
  {
    dtSensors.getAddress(owDeviceAddress, i);
    valueTemperature.identifier  = ID_TEMPERATURE_START + i;
    valueTemperature.value       = dtSensors.getTempC(owDeviceAddress);

    rfwrite(&valueTemperature, sizeof(valueTemperature)); // Send data via RF

#if SERIAL_DEBUG
  Serial.print("Temperature (C) ");
  printOneWireDeviceAddress(owDeviceAddress);
  Serial.print(" : ");
  Serial.println(valueTemperature.value);
  delay(1000);
#endif
  }


  
  //----------------------------------------------------
  // voltage level of the JeeNode
  //----------------------------------------------------
  // send voltage
  valueVoltage.value = voltageRead()/100.0;
  rfwrite(&valueVoltage, sizeof(valueVoltage)); // Send data via RF

#if SERIAL_DEBUG
  Serial.print("Voltage (V): ");
  Serial.println((float)(valueVoltage.value));
  delay(1000);
#endif

  //----------------------------------------------------
  // delay
  //----------------------------------------------------
#if SERIAL_DEBUG
  delay(1000);
#else
  Sleepy::loseSomeTime(60000); // sleep 1 minute
#endif
}

 
//--------------------------------------------------------------------------------------------------
// Send data via RF
//--------------------------------------------------------------------------------------------------
 static void rfwrite(const void* ptr, uint8_t len)
 {
   rf12_sleep(-1);      // wake up RF module
   while (!rf12_canSend())
   rf12_recvDone();
   rf12_sendStart(0, ptr, len);
   rf12_sendWait(2);    // wait for RF to finish sending while in standby mode
   rf12_sleep(0);       // put RF module to sleep
}

 
//--------------------------------------------------------------------------------------------------
// Reads current voltage
//--------------------------------------------------------------------------------------------------
int voltageRead()
{
  bitClear(PRR, PRADC);     // power up the ADC
  ADCSRA |= bit(ADEN);      // enable the ADC
  Sleepy::loseSomeTime(10);
  int voltage = map(analogRead(6), 0, 1023, 0, 660);
  ADCSRA &= ~ bit(ADEN);    // disable the ADC
  bitSet(PRR, PRADC);       // power down the ADC
  
  return voltage;
}


//--------------------------------------------------------------------------------------------------
// print OneWire device address
//--------------------------------------------------------------------------------------------------
void printOneWireDeviceAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

