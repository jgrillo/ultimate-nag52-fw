#include "sensors.h"
#include "driver/pcnt.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "pins.h"

#define PULSES_PER_REV 60 // N2 and N3 are 60 pulses per revolution
#define SAMPLES_PER_REVOLUTION 8
#define AVERAGE_SAMPLES 3

#define LOG_TAG "SENSORS"

#define CHECK_ESP_FUNC(x, msg, ...) \
res = x; \
if (res != ESP_OK) { \
    ESP_LOGE(LOG_TAG, msg, ##__VA_ARGS__); \
    return false; \
}   \

const pcnt_unit_t PCNT_N2_RPM = PCNT_UNIT_0;
const pcnt_unit_t PCNT_N3_RPM = PCNT_UNIT_1;

// PCNT configurations
const pcnt_config_t pcnt_cfg_n2 {
    .pulse_gpio_num = PIN_N2,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .lctrl_mode = PCNT_MODE_KEEP,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_DIS,
    .neg_mode = PCNT_COUNT_INC,
    .counter_h_lim = PULSES_PER_REV / SAMPLES_PER_REVOLUTION,
    .counter_l_lim = 0,
    .unit = PCNT_N2_RPM,
    .channel = PCNT_CHANNEL_0
};

const pcnt_config_t pcnt_cfg_n3 {
    .pulse_gpio_num = PIN_N3,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .lctrl_mode = PCNT_MODE_KEEP,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_DIS,
    .neg_mode = PCNT_COUNT_INC,
    .counter_h_lim = PULSES_PER_REV / SAMPLES_PER_REVOLUTION,
    .counter_l_lim = 0,
    .unit = PCNT_N3_RPM,
    .channel = PCNT_CHANNEL_0
};

struct RpmSampleData {
    uint64_t samples[AVERAGE_SAMPLES];
    uint64_t total;
    uint8_t sample_id;
    uint64_t delta;
    uint64_t last_time;
};

typedef struct {
    // Voltage in mV
    uint16_t v; 
    // ATF Temp in degrees C * 10
    int temp; 
} temp_reading_t;

#define NUM_TEMP_POINTS 22
const static temp_reading_t atf_temp_lookup[NUM_TEMP_POINTS] = {
//    mV, Temp(x10)
    {446, -400},
    {461, -300},
    {476, -200},
    {491, -100},
    {507, 0},
    {523, 100},
    {540, 200},
    {557, 300},
    {574, 400},
    {592, 500},
    {611, 600},
    {618, 700},
    {649, 800},
    {669, 900},
    {690, 1000},
    {711, 1100},
    {732, 1200},
    {755, 1300},
    {778, 1400},
    {802, 1500},
    {814, 1600},
    {851, 1700}
};

#define ADC_CHANNEL_VBATT adc2_channel_t::ADC2_CHANNEL_8
#define ADC_CHANNEL_ATF adc2_channel_t::ADC2_CHANNEL_9
#define ADC2_ATTEN ADC_ATTEN_11db
#define ADC2_WIDTH ADC_WIDTH_12Bit

esp_adc_cal_characteristics_t adc2_cal = {};

portMUX_TYPE n2_mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE n3_mux = portMUX_INITIALIZER_UNLOCKED;

RpmSampleData n2_samples = {
    .samples = {0},
    .total = 0,
    .sample_id = 0,
    .delta = 0,
    .last_time = 0
};

RpmSampleData n3_samples = {
    .samples = {0},
    .total = 0,
    .sample_id = 0,
    .delta = 0,
    .last_time = 0
};

#define WRITE_PULSES(MUX, SAMPLE) \
    portENTER_CRITICAL(MUX); \
    RpmSampleData* s = SAMPLE; \
    uint64_t now = esp_timer_get_time(); \
    s->total -= s->samples[s->sample_id]; \
    s->delta = (now - s->last_time); \
    s->samples[s->sample_id] = s->delta; \
    s->sample_id = ((s->sample_id)+1) % AVERAGE_SAMPLES; \
    s->total += s->delta; \
    s->last_time = now; \
    portEXIT_CRITICAL(MUX); \

static void IRAM_ATTR on_pcnt_overflow_n2(void* args) {
    WRITE_PULSES(&n2_mux, &n2_samples);
}

static void IRAM_ATTR on_pcnt_overflow_n3(void* args) {
    WRITE_PULSES(&n3_mux, &n3_samples);
}

bool Sensors::init_sensors(){
    esp_err_t res;
    CHECK_ESP_FUNC(gpio_set_direction(PIN_VBATT, GPIO_MODE_INPUT), "Failed to set PIN_VBATT to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_direction(PIN_ATF, GPIO_MODE_INPUT), "Failed to set PIN_ATF to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_direction(PIN_N2, GPIO_MODE_INPUT), "Failed to set PIN_N2 to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_direction(PIN_N3, GPIO_MODE_INPUT), "Failed to set PIN_N3 to Input! %s", esp_err_to_name(res))

    // Set RPM pins to pullup
    CHECK_ESP_FUNC(gpio_set_pull_mode(PIN_N2, GPIO_PULLUP_ONLY), "Failed to set PIN_N2 to Input! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(gpio_set_pull_mode(PIN_N3, GPIO_PULLUP_ONLY), "Failed to set PIN_N3 to Input! %s", esp_err_to_name(res))

    // Configure ADC2 for analog readings
    CHECK_ESP_FUNC(adc2_config_channel_atten(ADC_CHANNEL_VBATT, ADC2_ATTEN), "Failed to set ADC attenuation for PIN_ATF! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(adc2_config_channel_atten(ADC_CHANNEL_ATF, ADC2_ATTEN), "Failed to set ADC attenuation for PIN_VBATT! %s", esp_err_to_name(res))

    // Characterise ADC2
    esp_adc_cal_characterize(adc_unit_t::ADC_UNIT_2, ADC2_ATTEN, ADC2_WIDTH, 0, &adc2_cal);

    // Now configure PCNT to begin counting!
    CHECK_ESP_FUNC(pcnt_unit_config(&pcnt_cfg_n2), "Failed to configure PCNT for N2 RPM reading! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_unit_config(&pcnt_cfg_n3), "Failed to configure PCNT for N3 RPM reading! %s", esp_err_to_name(res))

    // Pause PCNTs unit configuration is complete
    CHECK_ESP_FUNC(pcnt_counter_pause(PCNT_N2_RPM), "Failed to pause PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_pause(PCNT_N3_RPM), "Failed to pause PCNT N3 RPM! %s", esp_err_to_name(res))

    // Clear their stored values (If present)
    CHECK_ESP_FUNC(pcnt_counter_clear(PCNT_N2_RPM), "Failed to clear PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_clear(PCNT_N3_RPM), "Failed to clear PCNT N3 RPM! %s", esp_err_to_name(res))

    // Setup filter to ignore ultra short pulses (possibly noise)
    // Using a value of 40 at 80Mhz APB_CLOCK = this will correctly filter noise up to 30,000RPM 
    CHECK_ESP_FUNC(pcnt_set_filter_value(PCNT_N2_RPM, 1000), "Failed to set filter for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_set_filter_value(PCNT_N3_RPM, 1000), "Failed to set filter for PCNT N3! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_filter_enable(PCNT_N2_RPM), "Failed to enable filter for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_filter_enable(PCNT_N3_RPM), "Failed to enable filter for PCNT N3! %s", esp_err_to_name(res))

    // Now install and register ISR interrupts
    CHECK_ESP_FUNC(pcnt_isr_service_install(0), "Failed to install ISR service for PCNT! %s", esp_err_to_name(res))

    CHECK_ESP_FUNC(pcnt_isr_handler_add(PCNT_N2_RPM, &on_pcnt_overflow_n2, nullptr), "Failed to add PCNT N2 to ISR handler! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_isr_handler_add(PCNT_N3_RPM, &on_pcnt_overflow_n3, nullptr), "Failed to add PCNT N3 to ISR handler! %s", esp_err_to_name(res))

    // Enable interrupts on hitting hlim on PCNTs
    CHECK_ESP_FUNC(pcnt_event_enable(PCNT_N2_RPM, pcnt_evt_type_t::PCNT_EVT_H_LIM), "Failed to register event for PCNT N2! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_event_enable(PCNT_N3_RPM, pcnt_evt_type_t::PCNT_EVT_H_LIM), "Failed to register event for PCNT N3! %s", esp_err_to_name(res))

    // Resume counting
    CHECK_ESP_FUNC(pcnt_counter_resume(PCNT_N2_RPM), "Failed to resume PCNT N2 RPM! %s", esp_err_to_name(res))
    CHECK_ESP_FUNC(pcnt_counter_resume(PCNT_N3_RPM), "Failed to resume PCNT N3 RPM! %s", esp_err_to_name(res))

    ESP_LOGI(LOG_TAG, "Sensors INIT OK!");
    return true;
}

inline uint32_t read_rpm(portMUX_TYPE* mux, RpmSampleData* sample) {
    uint64_t now = esp_timer_get_time();
    portENTER_CRITICAL(mux);
    if (now-sample->last_time > 100000) { // 100ms timeout
        portEXIT_CRITICAL(mux);
        return 0;
    }
#define NUMERATOR 1000000 * (PULSES_PER_REV / SAMPLES_PER_REVOLUTION)
    uint32_t res = NUMERATOR / (sample->total / AVERAGE_SAMPLES); 
    portEXIT_CRITICAL(mux);
    return res;
}

uint32_t Sensors::read_n2_rpm(){
    return read_rpm(&n2_mux, &n2_samples);
}

uint32_t Sensors::read_n3_rpm(){
    return read_rpm(&n3_mux, &n3_samples);
}

bool Sensors::read_vbatt(uint16_t *dest){
    uint32_t v;
    esp_err_t res = esp_adc_cal_get_voltage(adc_channel_t::ADC_CHANNEL_8, &adc2_cal, &v);
    if (res != ESP_OK) {
        ESP_LOGW("READ_VBATT", "Failed to query VBATT. %s", esp_err_to_name(res));
        return false;
    } else {
        // Vin = Vout(R1+R2)/R2
        *dest = v*5.54; // 5.54 = (100+22)/22
        return true;
    }
}

bool Sensors::read_atf_temp(int* dest){
    int raw;
    esp_err_t res = adc2_get_raw(ADC_CHANNEL_ATF, ADC2_WIDTH, &raw);
    if (res != ESP_OK) {
        ESP_LOGW("READ_VBATT", "Failed to query ATF temp. %s", esp_err_to_name(res));
        return false;
    } else {
        if (raw >= 3900) {
            return false; // Parking lock engaged, cannot read.
        }
        uint32_t tmp = esp_adc_cal_raw_to_voltage(raw, &adc2_cal);
        if (tmp < atf_temp_lookup[0].v) {
            *dest = atf_temp_lookup[0].temp;
            return true;
        } else if (tmp > atf_temp_lookup[NUM_TEMP_POINTS-1].v) {
            *dest = atf_temp_lookup[NUM_TEMP_POINTS-1].temp;
            return true;
        } else {
            for (uint8_t i = 0; i < NUM_TEMP_POINTS-1; i++) {
                // Found! Interpolate linearly to get a better estimate of ATF Temp
                if (atf_temp_lookup[i].v <= tmp && atf_temp_lookup[i+1].v >= tmp) {
                    float dx = tmp - atf_temp_lookup[i].v;
                    float dy = atf_temp_lookup[i+1].v - atf_temp_lookup[i].v;
                    *dest = atf_temp_lookup[i].temp + (atf_temp_lookup[i+1].temp-atf_temp_lookup[i].temp) * ((dx)/dy);
                    return true;
                }
            }
            return true;
        }
    }
}

bool Sensors::parking_lock_engaged(bool* dest){
    int raw;
    esp_err_t res = adc2_get_raw(ADC_CHANNEL_ATF, ADC2_WIDTH, &raw);
    if (res != ESP_OK) {
        ESP_LOGW("READ_VBATT", "Failed to query parking lock. %s", esp_err_to_name(res));
        return false;
    } else {
        *dest = raw >= 3900;
        return true;
    }
}