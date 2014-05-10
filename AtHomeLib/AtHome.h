
#ifndef AtHome_h
#define AtHome_h


//--------------------------------------------------------------------------------------------------
// JeeNode network
//--------------------------------------------------------------------------------------------------
#define AtHomeJNReceiverNodeID 	1               // RF12 node ID in the range 1-30
#define AtHomeJNMasterNodeID 	2               // RF12 node ID in the range 1-30

#define AtHomeJNNetworkGroup      210           // RF12 Network group
#define AtHomeJNFreq              RF12_868MHZ 	// Frequency of RFM12B module


//--------------------------------------------------------------------------------------------------
// The known value types
//--------------------------------------------------------------------------------------------------
typedef enum
{
    ValueType_Unknown       = 0,
    ValueType_WaterVolume   = 1,
    ValueType_Temperature   = 2,
    ValueType_Voltage       = 3
} valueType;


//--------------------------------------------------------------------------------------------------
// The sensor value sruct
//--------------------------------------------------------------------------------------------------
typedef struct
{
    int     identifier;     // the identifier of the sensor
    int     type;
    float   value;          // the value of the sensor
} AtHomeSensorValue;


#endif