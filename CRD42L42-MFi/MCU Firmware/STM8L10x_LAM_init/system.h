void pin_setup (void);

void timer2_init (void);
void timer2_service (void);

void timer3_init (void);
void wait_15ms (void);

// global variables

extern volatile uint8_t timer2_count;
extern uint8_t pending_launch;
extern uint8_t pending_en_ch;						// global flag indicating need to change enable state

