#ifndef __RADIO_H__
#define __RADIO_H__

typedef enum {
    RADIO_POWER_STATE_ON,
    RADIO_POWER_STATE_STANDBY,
    RADIO_POWER_STATE_SUSPEND
} radio_power_state_t;

struct adf4351_t;
struct cmx991_t;

typedef struct {
    struct adf4351_t* adf4351;
    struct cmx991_t* cmx991;
} radio_t;

void radio_init(radio_t* t);

void radio_destroy(radio_t* t);

// MORE OF A KERNEL IOCTL COMMAND
//void radio_reset(radio_t* t);
// MORE OF A KERNEL PM COMMAND
//void radio_set_power_state(radio_t* t, radio_power_state_t state);

// IDEALLY SETS THE KERNEL DRIVER
void radio_set_center_frequency(radio_t* t);

// IDEALLY ASKS THE KERNEL DRIVER
double radio_get_center_frequency(radio_t* t);

#endif /* __RADIO_H__ */
