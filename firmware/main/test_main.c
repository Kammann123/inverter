#include <stdio.h>
#include "driver/mcpwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "esp_task_wdt.h"

#define MCPWM_UNIT      MCPWM_UNIT_0
#define MCPWM_TIMER     MCPWM_TIMER_0
#define MCPWM_CHA       MCPWM0A 
#define MCPWM_CHB       MCPWM0B
#define MCPWM_GPIO_1A    4
#define MCPWM_GPIO_1B    2

#define PWM_FREQUENCY   1000

#define DELAY_MS        100

#define RED     100 
#define FED     100

bool change = false;

#define TEST_LENGTH 11
float test[TEST_LENGTH] = {
    0.5,
    10,
    20,
    30,
    40, 
    50,
    60,
    70,
    80,
    90,
    100
};

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
uint16_t index = 0;

static void IRAM_ATTR isr_handler0(void* arg)
{
    uint32_t mcpwm_int_status = MCPWM0.int_st.val;
    if (mcpwm_int_status & (BIT(6)))
    {
        change = true;
        mcpwm_stop(MCPWM_UNIT, MCPWM_TIMER);
    }
    MCPWM0.int_clr.val = mcpwm_int_status;
}

void app_main(void)
{
    esp_task_wdt_init(30, false);

    mcpwm_gpio_init(MCPWM_UNIT, MCPWM_CHA, MCPWM_GPIO_A);
    mcpwm_gpio_init(MCPWM_UNIT, MCPWM_CHB, MCPWM_GPIO_B);
    mcpwm_config_t pwm_config = {
        .frequency = PWM_FREQUENCY,
        .cmpr_a = 0,
        .cmpr_b = 0,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0
    };
    mcpwm_init(MCPWM_UNIT, MCPWM_TIMER, &pwm_config);
    mcpwm_deadtime_enable(MCPWM_UNIT, MCPWM_TIMER, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, RED, FED);
    MCPWM0.int_ena.timer0_tep_int_ena = true;
    mcpwm_isr_register(MCPWM_UNIT, isr_handler0, NULL, ESP_INTR_FLAG_IRAM, NULL);
    mcpwm_set_frequency(MCPWM_UNIT, MCPWM_TIMER, PWM_FREQUENCY);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER, MCPWM_OPR_A, sine[0]);
    mcpwm_start(MCPWM_UNIT, MCPWM_TIMER);
    
    while (true)
    {
        if (change == true)
        {
            change = false;
            mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER, MCPWM_OPR_A, sine[index++]);
            if (index >= SINE_LENGTH)
                index = 0;
            mcpwm_start(MCPWM_UNIT, MCPWM_TIMER);
        }
    }
}