// Standard Libraries
#include <stdio.h>

// ESP-IDF SDK Libraries
#include "driver/mcpwm.h"
#include "driver/adc.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

// Project Libraries
#include "sine.h"

// Constant Definitions
#define PAH_GPIO        4
#define PAL_GPIO        2
#define PBH_GPIO        5
#define PBL_GPIO        18
#define PCH_GPIO        19
#define PCL_GPIO        21
#define ANALOG_CHANNEL  ADC1_CHANNEL_6

// Inverter Parameters
#define MIN_FREQUENCY       40
#define MAX_FREQUENCY       60
#define SAMPLE_PERIOD_US    1000000
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
    bool                dutyChangeFlag;
    uint32_t            dutyTableIndex;
} phase_t;

typedef struct
{
    phase_t pa;
    phase_t pb;
    phase_t pc;
    float scale;
} inverter_t;

// Function Prototypes
void phase_init(phase_t* instance);
void phase_update_duty(phase_t* instance, float ma);
void inverter_update_frequency(inverter_t* inverter);
void IRAM_ATTR mcpwm_isr(void* arg);

static const char* TAG = "Inverter";

void app_main(void)
{
    int64_t currentTime = 0;
    int64_t lastTime = 0;

    // Disable the tasks' watchdog
    // esp_task_wdt_init(5, false);

    // Log initial message
    ESP_LOGI(TAG, "Inverter ON");

    // Initialize the ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ANALOG_CHANNEL, ADC_ATTEN_DB_11);

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
        },
        .scale = 1.0
    };

    // Initialize the phases
    phase_init(&inverter.pa);
    phase_init(&inverter.pb);
    phase_init(&inverter.pc);
    
    // Register the ISR
    mcpwm_isr_register(MCPWM_UNIT_0, mcpwm_isr, &inverter, ESP_INTR_FLAG_IRAM, NULL);
    
    while (true)
    {
        // Sampling the input potentiometer and calculating the 
        // factor to scale the frequency and the amplitude
        currentTime = esp_timer_get_time();
        if ((currentTime - lastTime) > SAMPLE_PERIOD_US)
        {
            lastTime = currentTime;
            int sample = adc1_get_raw(ANALOG_CHANNEL);
            inverter.scale = sample / 4095.0;
            inverter_update_frequency(&inverter);
            // ESP_LOGI(TAG, "Scale: %f", inverter.scale);
        }

        // Update the duty cycle
        phase_update_duty(&inverter.pa, inverter.scale);
        phase_update_duty(&inverter.pb, inverter.scale);
        phase_update_duty(&inverter.pc, inverter.scale);

        // Reset the WDT
        // esp_task_wdt_init(5, false);
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

void phase_update_duty(phase_t* instance, float ma)
{
    if (instance != NULL)
    {
        phase_t* phase = (phase_t*)instance;
        if (phase->dutyChangeFlag == true)
        {
            phase->dutyChangeFlag = false;
            float duty = (sine[phase->dutyTableIndex++] - 50.0) * ma + 50.0;
            mcpwm_set_duty(phase->unit, phase->timer, MCPWM_OPR_A, duty); // Hardcoded ?
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
    uint32_t mcpwm_int_status = MCPWM0.int_st.val; // Hardcoded
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
    MCPWM0.int_clr.val = mcpwm_int_status; // Hardcoded
}

void inverter_update_frequency(inverter_t* inverter)
{
    uint32_t sineFrequency = MIN_FREQUENCY + (MAX_FREQUENCY - MIN_FREQUENCY) * inverter->scale;
    uint32_t pwmFrequency = sineFrequency * SINE_LENGTH;
    mcpwm_set_frequency(inverter->pa.unit, inverter->pa.timer, pwmFrequency);
    mcpwm_set_frequency(inverter->pb.unit, inverter->pb.timer, pwmFrequency);
    mcpwm_set_frequency(inverter->pc.unit, inverter->pc.timer, pwmFrequency);
}