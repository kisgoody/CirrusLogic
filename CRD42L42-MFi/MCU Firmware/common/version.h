#define HW_MAJOR_VER		0x01
#define HW_MINOR_VER		0x00
#define HW_REVISION			0x00

#define FW_MAJOR_VER		0x01
#define FW_MINOR_VER		0x01
#define FW_REVISION			0x01
// In Octobeer of 2015, the strlen function was causing problems
// The length of these strings is hard-coded into packet.c
// Do not change the length of these strings without changing the length
#define INFO_NAME			"CRD42L42-MFi"
#define INFO_MANF			"Cirrus Logic"
#define INFO_MODEL			"CS42L42"
#define INFO_PREF_APP		"com.cirrus.CRD42L42-MFi"
#define INFO_EA_NAME		"com.cirrus.CRD42L42-MFi"

// function declarations

void pin_setup (void);

void timer2_init (void);
void timer2_service (void);

void timer3_init (void);
void wait_15ms (void);

// global variables

extern volatile uint8_t timer2_count;
extern uint8_t pending_launch;
extern uint8_t pending_en_ch;						// global flag indicating need to change enable state

