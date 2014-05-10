#include <JeeLib.h>
#include <SerialCommand.h>
#include <RCSwitch.h>
#include <AtHome.h>
#include <util/crc16.h>
#include <util/parity.h>


//--------------------------------------------------------------------------------------------------
// Supported send commands:
//
// FS20           F houseId deviceId 0/1
// Elro           E houseId deviceId 0/1
//
//
// Supported receive events
//
// Elro           E houseId deviceId 0/1
// SensorValue    SV deviceId type value(float)
//--------------------------------------------------------------------------------------------------



//----------------------------------------------------
// PINs
//----------------------------------------------------
#define PIN_RCSWITCH_TRANSMIT   4  // 433MhZ Transmitter
#define PIN_RCSWITCH_RECEIVE    1  // 433MhZ Receiver on interrupt 1 (PIN 3)


// The serial command object for parsing commands
SerialCommand sCmd;
RCSwitch rcSwitch = RCSwitch();

//----------------------------------------------------
//Data Structure to be received
//----------------------------------------------------
AtHomeSensorValue sensorValue;

//----------------------------------------------------
// FS20
//----------------------------------------------------
static void ookPulse(int on, int off) {
  rf12_onOff(1);
  delayMicroseconds(on + 150);
  rf12_onOff(0);
  delayMicroseconds(off - 200);
}

static void fs20sendBits(word data, byte bits) {
  if (bits == 8) {
    ++bits;
    data = (data << 1) | parity_even_bit(data);
  }
  for (word mask = bit(bits - 1); mask != 0; mask >>= 1) {
    int width = data & mask ? 600 : 400;
    ookPulse(width, width);
  }
}

static void fs20cmd(word house, byte addr, byte cmd) {
  byte sum = 6 + (house >> 8) + house + addr + cmd;
  for (byte i = 0; i < 3; ++i) {
    fs20sendBits(1, 13);
    fs20sendBits(house >> 8, 8);
    fs20sendBits(house, 8);
    fs20sendBits(addr, 8);
    fs20sendBits(cmd, 8);
    fs20sendBits(sum, 8);
    fs20sendBits(0, 1);
    delay(10);
  }
}



//--------------------------------------------------------------------------------------------------
// all command handlers
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// This gets set as the default handler, and it is called if no other command matches.
//--------------------------------------------------------------------------------------------------
void cmd_Unkown(const char *command) {
  Serial.println("@Home: unknown command");
}

//--------------------------------------------------------------------------------------------------
// send FS20 command
//--------------------------------------------------------------------------------------------------
void cmd_FS20() 
{
  char *arg = sCmd.next();
  if (arg == NULL) 
    return;

//  Serial.println("cmd_FS20");

  long int value = strtoul(arg, NULL, 16);
  byte cmd = value & 0x000000FF;
  value >>= 8;
  byte deviceid = value & 0x000000FF;
  value >>= 8;
  word houseid = value;

  rf12_initialize(0, RF12_868MHZ);
  fs20cmd(houseid, deviceid, cmd);
  rf12_config(0); // restore normal packet listening mode
}

//--------------------------------------------------------------------------------------------------
// send Elro command
//--------------------------------------------------------------------------------------------------
void cmd_ELRO()
{
  char *house = sCmd.next();
  if (house == NULL) 
    return;
  char *device = sCmd.next();
  if (device == NULL) 
    return;
  char *cmd = sCmd.next();
  if (cmd == NULL) 
    return;

  if ( strcmp(cmd, "1") == 0 )
    rcSwitch.switchOn(house, device);
  else
    rcSwitch.switchOff(house, device);
}

//--------------------------------------------------------------------------------------------------
// Handle a received Elro command 
//--------------------------------------------------------------------------------------------------
void handle_ELRO_Receive(unsigned long value)
{
    if (value == 0) 
      return;
    
    unsigned long  onoff = value & 0x000F;
    value =  value >> 4;

    unsigned long  device  = value & 0x03FF;
    if ( device == 0b0101010101 )
      return;
      
    value =  value >> 10;
    unsigned long  house = value & 0x03FF;

    char code[6];
    code[5] = 0;
    char mapOnOff[2];
    mapOnOff[0] = '1';
    mapOnOff[1] = '0';

    for ( int b = 4; b >= 0; b--)
    {
      code[b] = mapOnOff[house & 0x0003];
      house =  house >> 2;
    }
    
    // send string of format 'E housecode deviceid value', e.g. 'E 10011 00100 1'
    Serial.print("E ");
    Serial.print(code);
    Serial.print(" ");
    for ( int b = 4; b >= 0; b--)
    {
      code[b] = mapOnOff[device & 0x0003];
      device =  device >> 2;
    }
    Serial.print(code);
    Serial.print(" ");
    if ( onoff == 1 )
      Serial.println("1");
    else
      Serial.println("0");
}

//--------------------------------------------------------------------------------------------------
// setup
//--------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(57600);
  Serial.println("@Home online");

  // setup serial command handler
  sCmd.addCommand("F", cmd_FS20);
  sCmd.addCommand("E", cmd_ELRO);
  sCmd.setDefaultHandler(cmd_Unkown);
  
  // setup 433MHz transmitter/receiver
  rcSwitch.enableTransmit(PIN_RCSWITCH_TRANSMIT);
  rcSwitch.enableReceive(PIN_RCSWITCH_RECEIVE);

  // setup JeeNode communication
  rf12_initialize(AtHomeJNReceiverNodeID, AtHomeJNFreq, AtHomeJNNetworkGroup);
}


//--------------------------------------------------------------------------------------------------
// loop
//--------------------------------------------------------------------------------------------------
void loop()
{
  // handle serial commands
  sCmd.readSerial();

  // handle 433Mhz receive
  if (rcSwitch.available()) 
  {
    unsigned long value = rcSwitch.getReceivedValue();
    handle_ELRO_Receive(value);
    rcSwitch.resetAvailable();
  }

  // data received on RF12?
  if (rf12_recvDone() && rf12_crc == 0) 
  {
    // send string of format 'SV identifier type value'
    const AtHomeSensorValue* p = (const AtHomeSensorValue*) rf12_data;
    Serial.print("SV ");
    Serial.print(p->identifier);
    Serial.print(" ");
    Serial.print(p->type);
    Serial.print(" ");
    Serial.println(p->value);
  }
}

