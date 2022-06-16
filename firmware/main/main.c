#include <stdio.h>

#include "driver/mcpwm.h"

typedef struct {
    mcpwm_unit_t        unit;           // MCPWM Unit
    mcpwm_timer_t       timer;          // MCPWM Timer
    mcpwm_io_signals_t  highChannel;    // High side PWM channel
    mcpwm_io_signals_t  lowChannel;     // Low side PWM channel
} inverter_phase_t;

void inverter_phase_init(inverter_phase_t* p);

void app_main(void)
{
    // Disable the tasks' watchdog
    esp_task_wdt_init(30, false);

}