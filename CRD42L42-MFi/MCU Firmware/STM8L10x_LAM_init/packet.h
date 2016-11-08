// pre-compiler definitions

#define START_OF_PACKET			0x55				// start of packet field
#define MAX_PACKET_LENGTH		132					// maximum packet length (in bytes)

#define EA_WRITE				0xEA				// external accessory protocol write key
#define EA_READ					0xEB				// external accessory protocol read key

#define accReady									0x01
#define lamReady									0x02

#define accConfigurationInformation					0x03
#define accNameInformation							0x04
#define accManufacturerInformation					0x05
#define accModelInformation							0x06
#define accSerialNumberInformation					0x07
#define accPreferredAppBundleIdentifierInformation	0x08
#define accExternalAccessoryProtocolNameInformation	0x09
#define accHIDComponentInformation					0x0A
#define accAudioTerminalInformation					0x0B

#define lamInformation								0x10
#define lamAudioTerminalInformation					0x11
#define lamExternalAccessoryProtocolSessionData		0x12
#define lamHIDReport															0x13
#define accVersionInformation											0x14 //new
#define accExternalAccessoryProtocolSessionData		0x20
#define accHIDReport								0x21
#define accRequestAppLaunch							0x22
#define accAudioTerminalStateInformation			0x23
#define accPassthroughPowerSourceInformation		0x24  //only if accessory has battery
#define lamVersionInformation											0x25 //new

#define accSpecialInitWait							0xF1
#define accSpecialConfigWait						0xF2
#define accSpecialOperational						0xFE
#define accSpecialUnknown							0xFF

// function declarations

uint8_t next_packet (uint8_t current_packet);
int8_t packet_checksum (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload);
void packet_process (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload);
void packet_send (uint8_t packet_length, uint8_t packet_type, uint8_t *packet_payload);
uint8_t packet_receive (uint8_t packet_index, uint8_t packet_byte, uint8_t *packet_length, uint8_t *packet_type, uint8_t *packet_payload);

// global variables

extern uint8_t LAM_busy;
extern uint8_t EA_open;
extern uint8_t pending_launch;
extern uint8_t LED_ON;
extern uint8_t WAITING;
extern uint8_t Active_Config;
extern volatile uint8_t timer2_count;
extern uint8_t pending_en_ch;						// global flag indicating need to change enable state
