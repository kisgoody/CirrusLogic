#include "stm8l10x_conf.h"
#include "dev_config.h"

//FW start (FW interrupt table address)
#define FW_START_ADDR	0x88C0ul

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
	unsigned char interrupt_instruction;
	interrupt_handler_t interrupt_handler;
};
