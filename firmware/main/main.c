// Standard Libraries
#include <stdio.h>

// ESP-IDF SDK Libraries
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Constant Definitions
#define PAH_GPIO        4
#define PAL_GPIO        2
#define PBH_GPIO        5
#define PBL_GPIO        18
#define PCH_GPIO        19
#define PCL_GPIO        21

#define DEFAULT_FREQUENCY   1980
#define DELAY_MS            100
#define RED                 100 
#define FED                 100 

// Data Types
typedef struct
{
    mcpwm_unit_t        unit;
    mcpwm_dev_t*        device;
    mcpwm_timer_t       timer;
    mcpwm_io_signals_t  highChannel;
    uint32_t            highPin;
    mcpwm_io_signals_t  lowChannel;
    uint32_t            lowPin;
    uint32_t            red;
    uint32_t            fed;
    bool        dutyChangeFlag;
    uint32_t    dutyTableIndex;
} phase_t;

typedef struct
{
    phase_t pa;
    phase_t pb;
    phase_t pc;
} inverter_t;

// Function Prototypes
void phase_init(phase_t* instance);
void phase_update_duty(phase_t* instance);
void IRAM_ATTR mcpwm_isr(void* arg);

#define SINE_LENGTH 33
float sine[SINE_LENGTH] = {
    50.0,
    59.755,
    69.134,
    77.779,
    85.355,
    91.573,
    96.194,
    99.039,
    100.0,
    99.039,
    96.194,
    91.573,
    85.355,
    77.779,
    69.134,
    59.755,
    50.0,
    40.245,
    30.866,
    22.221,
    14.645,
    8.427,
    3.806,
    0.961,
    0.0,
    0.961,
    3.806,
    8.427,
    14.645,
    22.221,
    30.866,
    40.245,
    50.0
};

void app_main(void)
{
    // Disable the tasks' watchdog
    esp_task_wdt_init(30, false);

    // Instance the inverter structure
    inverter_t inverter = 
    {
        .pa = {
            .unit = MCPWM_UNIT_0,
            .device = &MCPWM0,
            .timer = MCPWM_TIMER_0,
            .highChannel = MCPWM0A,
            .highPin = PAH_GPIO,
            .lowChannel = MCPWM0B,
            .lowPin = PAL_GPIO,
            .red = RED,
            .fed = FED,
            .dutyChangeFlag = false,
            .dutyTableIndex = 0
        },
        .pb = {
            .unit = MCPWM_UNIT_0,
            .device = &MCPWM0,
            .timer = MCPWM_TIMER_1,
            .highChannel = MCPWM1A,
            .highPin = PBH_GPIO,
            .lowChannel = MCPWM1B,
            .lowPin = PBL_GPIO,
            .red = RED,
            .fed = FED,
            .dutyChangeFlag = false,
            .dutyTableIndex = 11
        },
        .pc = {
            .unit = MCPWM_UNIT_0,
            .device = &MCPWM0,
            .timer = MCPWM_TIMER_2,
            .highChannel = MCPWM2A,
            .highPin = PCH_GPIO,
            .lowChannel = MCPWM2B,
            .lowPin = PCL_GPIO,
            .red = RED,
            .fed = FED,
            .dutyChangeFlag = false,
            .dutyTableIndex = 22
        }
    };

    phase_init(&inverter.pa);
    phase_init(&inverter.pb);
    phase_init(&inverter.pc);
    
    mcpwm_isr_register(MCPWM_UNIT_0, mcpwm_isr, &inverter, ESP_INTR_FLAG_IRAM, NULL);
    
    while (true)
    {
        phase_update_duty(&inverter.pa);
        phase_update_duty(&inverter.pb);
        phase_update_duty(&inverter.pc);
    }
}

// Function Definitions
void phase_init(phase_t* instance)
{
    if (instance != NULL)
    {
        mcpwm_gpio_init(instance->unit, instance->highChannel, instance->highPin);
        mcpwm_gpio_init(instance->unit, instance->lowChannel, instance->lowPin);
        mcpwm_config_t pwm_config = {
            .frequency = DEFAULT_FREQUENCY,
            .cmpr_a = 0,
            .cmpr_b = 0,
            .counter_mode = MCPWM_UP_COUNTER,
            .duty_mode = MCPWM_DUTY_MODE_0
        };
        mcpwm_init(instance->unit, instance->timer, &pwm_config);
        mcpwm_deadtime_enable(instance->unit, instance->timer, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, instance->red, instance->fed);
        if (instance->timer == MCPWM_TIMER_0)
        {
            instance->device->int_ena.timer0_tep_int_ena = true;
        }
        else if (instance->timer == MCPWM_TIMER_1)
        {
            instance->device->int_ena.timer1_tep_int_ena = true;
        }
        else if(instance->timer == MCPWM_TIMER_2)
        {
            instance->device->int_ena.timer2_tep_int_ena = true;
        }
        mcpwm_start(instance->unit, instance->timer);
    }
}

void phase_update_duty(phase_t* instance)
{
    if (instance != NULL)
    {
        phase_t* phase = (phase_t*)instance;
        if (phase->dutyChangeFlag == true)
        {
            phase->dutyChangeFlag = false;
            mcpwm_set_duty(phase->unit, phase->timer, MCPWM_OPR_A, sine[phase->dutyTableIndex++]);
            if (phase->dutyTableIndex >= SINE_LENGTH)
            {
                phase->dutyTableIndex = 0;
            }
        }
    }
}

void IRAM_ATTR mcpwm_isr(void* arg)
{
    inverter_t* inverter = (inverter_t*)arg;
    uint32_t mcpwm_int_status = MCPWM0.int_st.val;
    if (mcpwm_int_status & (BIT(6)))
    {
        inverter->pa.dutyChangeFlag = true;
    }
    if (mcpwm_int_status & (BIT(7)))
    {
        inverter->pb.dutyChangeFlag = true;
    }
    if (mcpwm_int_status & (BIT(8)))
    {
        inverter->pc.dutyChangeFlag = true;
    }
    MCPWM0.int_clr.val = mcpwm_int_status;
}