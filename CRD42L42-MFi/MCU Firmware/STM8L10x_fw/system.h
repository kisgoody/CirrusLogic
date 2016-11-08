uint8_t next_button_state (uint8_t current_button_state);
uint8_t next_plug_state (uint8_t current_sp_button_st);

void pin_setup (void);

void timer2_init (void);
void timer2_service (void);

void timer3_init (void);
void wait_15ms (void);

void codec_init (void);
void codec_pwr_up (void);
void codec_pwr_dn (void);
void ADC_pwr_up (void);
void ADC_pwr_dn (void);
void hs_type_det (void);
void hs_manual (void);

void codec_reg_write (uint8_t reg, uint8_t value);
void codec_reg_write_mult (uint8_t reg, uint8_t length, uint8_t *payload);

uint8_t codec_reg_read (uint8_t reg);
void codec_reg_read_mult (uint8_t reg, uint8_t length, uint8_t *payload);

// global variables

extern volatile uint8_t timer2_count;
extern uint8_t pending_launch;
extern uint8_t DAC_EN;
extern uint8_t ADC_EN;
extern uint8_t pending_en_ch;						// global flag indicating need to change enable state
extern uint8_t Tip_plugged;	  					// global flag indicating status of 3.5mm jack
extern uint8_t HP_Only;								  // global flag indicating headphone only (no mic)
extern uint8_t Optical_Only;						  // global flag indicating optical only (no capability)

