#include "gearbox.h"
#include "scn.h"
#include "nvs/eeprom_config.h"

const float diff_ratio_f = (float)DIFF_RATIO / 1000.0;

#define max(a,b) \
  ({ \
    typeof (a) _a = (a); \
    typeof (b) _b = (b); \
    _a > _b ? _a : _b; \
  })

#define CLAMP(value, min, max) \
    if (value < min) { \
        value = min; \
    } else if (value >= max) { \
        value = max-1; \
    } \

// ONLY FOR FORWARD GEARS!
int calc_input_rpm_from_req_gear(int output_rpm, GearboxGear req_gear) {
    switch (req_gear) {
        case GearboxGear::First:
            return output_rpm*RAT_1;
        case GearboxGear::Second:
            return output_rpm*RAT_2;
        case GearboxGear::Third:
            return output_rpm*RAT_3;
        case GearboxGear::Fourth:
            return output_rpm*RAT_4;
        case GearboxGear::Fifth:
            return output_rpm*RAT_5;
        default:
            return 0;
    }
}

// RPM vs MPC pressure when driving (0-8000RPM)
const uint16_t mpc_hold_pressure[9] = {300, 320, 340, 360, 370, 380, 390, 400, 400};

uint16_t find_mpc_hold_pressure(int engine_rpm, int temp_raw) {
    if (engine_rpm < 0) {
        return mpc_hold_pressure[0] * find_temp_multiplier(temp_raw);
    } else if (engine_rpm > 8000) {
        return mpc_hold_pressure[8] * find_temp_multiplier(temp_raw);
    } else {
        int min = engine_rpm/1000;
        int max = min+1;
        float dy = mpc_hold_pressure[max] - mpc_hold_pressure[min];
        float dx = (max-min)*1000;
        return (mpc_hold_pressure[min] + ((dy/dx)) * (engine_rpm-(min*1000)))  * find_temp_multiplier(temp_raw);
    }
}

uint16_t find_mpc_driving_pressure(SensorData* sensors, GearboxGear g) {
    switch (g) {
        case GearboxGear::First:
            return find_mpc_pressure(map_1_2, sensors, 1.0);
        case GearboxGear::Second:
            return find_mpc_pressure(map_2_3, sensors, 1.0);
        case GearboxGear::Third:
            return find_mpc_pressure(map_3_4, sensors, 1.0);
        case GearboxGear::Fourth:
            return find_mpc_pressure(map_4_5, sensors, 1.0);
        case GearboxGear::Fifth:
            return find_mpc_pressure(map_5_4, sensors, 1.0);
        default:
            return 100;
    }
}

Gearbox::Gearbox() {
    this->current_profile = nullptr;
    egs_can_hal->set_drive_profile(GearboxProfile::Underscore); // Uninitialized
    this->profile_mutex = portMUX_INITIALIZER_UNLOCKED;
    this->sensor_data = SensorData {
        .input_rpm = 0,
        .engine_rpm = 0,
        .output_rpm = 0,
        .voltage = 12000,   
        .pedal_pos = 0,
        .atf_temp = 0,
        .static_torque = 0,
        .max_torque = 0,
        .min_torque = 0,
        .tcc_slip_rpm = 0,
        .last_shift_time = 0,
        .current_timestamp_ms = (uint64_t)(esp_timer_get_time()/1000),
        .is_braking = false
    };
    this->tcc = new TorqueConverter();
}

void Gearbox::set_profile(AbstractProfile* prof) {
    if (prof != nullptr) { // Only change if not nullptr!
        portENTER_CRITICAL(&this->profile_mutex);
        this->current_profile = prof;
        // Set CAN display
        egs_can_hal->set_drive_profile(this->current_profile->get_profile());
        this->min_fwd_gear = prof->get_start_gear();
        portEXIT_CRITICAL(&this->profile_mutex);
    }
}

bool Gearbox::start_controller() {
    xTaskCreatePinnedToCore(Gearbox::start_controller_internal, "GEARBOX", 32768, (void*)this, 10, nullptr, 1);
    return true;
}

GearboxGear gear_from_idx(uint8_t idx) {
    switch (idx) {
        case 1:
            return GearboxGear::First;
        case 2:
            return GearboxGear::Second;
        case 3:
            return GearboxGear::Third;
        case 4:
            return GearboxGear::Fourth;
        case 5:
            return GearboxGear::Fifth;
        case 6:
            return GearboxGear::Reverse_First;
        case 7:
            return GearboxGear::Reverse_Second;
        default:
            return GearboxGear::SignalNotAvaliable;
    }
}

bool is_controllable_gear(GearboxGear g) {
    switch (g) {
        case GearboxGear::Reverse_First:
        case GearboxGear::Reverse_Second:
        case GearboxGear::First:
        case GearboxGear::Second:
        case GearboxGear::Third:
        case GearboxGear::Fourth:
        case GearboxGear::Fifth:
            return true;
        case GearboxGear::Park:
        case GearboxGear::SignalNotAvaliable:
        case GearboxGear::Neutral:
        default:
            return false;
    }
}

bool is_fwd_gear(GearboxGear g) {
    switch (g) {
        case GearboxGear::Reverse_First:
        case GearboxGear::Reverse_Second:
            return false;
        case GearboxGear::First:
        case GearboxGear::Second:
        case GearboxGear::Third:
        case GearboxGear::Fourth:
        case GearboxGear::Fifth:
            return true;
        case GearboxGear::Park:
        case GearboxGear::SignalNotAvaliable:
        case GearboxGear::Neutral:
        default:
            return false;
    }
}

const char* gear_to_text(GearboxGear g) {
    switch (g) {
        case GearboxGear::Reverse_First:
            return "R1";
        case GearboxGear::Reverse_Second:
            return "R2";
        case GearboxGear::First:
            return "D1";
        case GearboxGear::Second:
            return "D2";
        case GearboxGear::Third:
            return "D3";
        case GearboxGear::Fourth:
            return "D4";
        case GearboxGear::Fifth:
            return "D5";
        case GearboxGear::Park:
            return "P";
        case GearboxGear::SignalNotAvaliable:
            return "SNA";
        case GearboxGear::Neutral:
            return "N";
        default:
            return "";
    }
}

void Gearbox::inc_gear_request() {
    this->ask_upshift = true;
    this->ask_downshift = false;
}

void Gearbox::dec_gear_request() {
    this->ask_upshift = false;
    this->ask_downshift = true;
}

GearboxGear next_gear(GearboxGear g) {
    switch (g) {
        case GearboxGear::First:
            return GearboxGear::Second;
        case GearboxGear::Second:
            return GearboxGear::Third;
        case GearboxGear::Third:
            return GearboxGear::Fourth;
        case GearboxGear::Fourth:
            return GearboxGear::Fifth;
        case GearboxGear::Park:
        case GearboxGear::SignalNotAvaliable:
        case GearboxGear::Neutral:
        case GearboxGear::Reverse_First:
        case GearboxGear::Reverse_Second:
        default:
            return g;
    }
}

GearboxGear prev_gear(GearboxGear g) {
    switch (g) {
        case GearboxGear::Second:
            return GearboxGear::First;
        case GearboxGear::Third:
            return GearboxGear::Second;
        case GearboxGear::Fourth:
            return GearboxGear::Third;
        case GearboxGear::Fifth:
            return GearboxGear::Fourth;
        case GearboxGear::First:
        case GearboxGear::Park:
        case GearboxGear::SignalNotAvaliable:
        case GearboxGear::Neutral:
        case GearboxGear::Reverse_First:
        case GearboxGear::Reverse_Second:
        default:
            return g;
    }
}

#define SHIFT_TIMEOUT_MS 3000 // If a shift hasn't occurred after this much time, we assume shift has failed!
#define SHIFT_DELAY_MS 20
#define SHIFT_CONFIRM_COUNT 80/SHIFT_DELAY_MS // 80ms in gear for confirm

/**
 * @brief Used to shift between forward gears
 * 
 * @param init_spc Initial SPC value
 * @param init_mpc Initial MPC value
 * @param shift_solenoid Shift solenoid to open up to do the shift
 * @param target_shift_duration_ms Target time in ms for the gear change to occur
 * @param targ_gear Target gear that we are shifting to
 * @return uint16_t - The actual time taken to shift gears. This is fed back into the adaptation network so it can better meet 'target_shift_duration_ms'
 */
ShiftResponse Gearbox::elapse_shift(ProfileGearChange req_lookup, AbstractProfile* profile, Solenoid* shift_solenoid, uint8_t curr_gear, uint8_t targ_gear) {
    ESP_LOGI("SHIFTER", "SHIFT START %d -> %d", curr_gear, targ_gear);
    ShiftData s;
    if (profile != nullptr) {
        s = profile->get_shift_data(req_lookup, &this->sensor_data);
    } else {
        s = DEFAULT_SHIFT_DATA;
    }
    // Prime MPC
    uint16_t initial_mpc = s.mpc_pwm;
    uint16_t initial_spc = s.spc_pwm;
    s.shift_firmness = 1.0;
    this->tcc->on_shift_start(sensor_data.current_timestamp_ms, curr_gear > targ_gear, s.shift_firmness);
    sol_mpc->write_pwm_percent_with_voltage(s.mpc_pwm, sensor_data.voltage);
    sol_spc->write_pwm_percent_with_voltage(s.spc_pwm, this->sensor_data.voltage); // Open SPC
    shift_solenoid->write_pwm_percent_with_voltage(1000, this->sensor_data.voltage); // Start shifting
    uint32_t elapsed = 0; // Counter for shift timing
    ShiftData sd;
    // Change gears begin
    int start_rpm = this->sensor_data.input_rpm;    
    int max_d_rpm = 0;
    int min_d_rpm = INT_MAX;
    int avg_d_rpm = 0;
    int rpm_samples = 0;
    int last_rpm = this->sensor_data.input_rpm;
    bool shift_measure_complete = false;
    bool flare_compensation = false;
    //sol_tcc->write_pwm(0); // Unlock the converter
    while(elapsed <= SHIFT_TIMEOUT_MS) {
        if (!(curr_gear == 1 || targ_gear == 5)) { // Disable detection for 1-2 4-5
            if (this->sensor_data.input_rpm > start_rpm+100 && targ_gear > curr_gear) {
                // FLARING!
                this->flaring = true;
                if (!flare_compensation) {
                    flare_compensation = true;
                    initial_mpc *= 0.90;
                    initial_spc *= 0.90;
                }
            }
        }
        if (profile != nullptr) {
            sd = profile->get_shift_data(req_lookup, &this->sensor_data);   
        }
        sol_mpc->write_pwm_percent_with_voltage(((initial_mpc*9) + (sd.mpc_pwm))/10, this->sensor_data.voltage);
        sol_spc->write_pwm_percent_with_voltage(((initial_spc*9) + (sd.spc_pwm))/10, this->sensor_data.voltage); // Open SPC
        // Check using actual gear ratios (high speed moving, easiest way)
        if (this->est_gear_idx == targ_gear) {
            shift_measure_complete = true;
            break;
        } else if (sensor_data.output_rpm < 100 && elapsed >= 750) { // Fix for stationary shifts
            shift_measure_complete = false; // Cannot measure for adaptation (Standstill)
            break;
        }
        else {
            // Measure D_RPM
            int new_rpm = this->sensor_data.input_rpm;
            int delta = new_rpm - last_rpm;
            avg_d_rpm += delta;
            if (delta < min_d_rpm) {
                min_d_rpm = delta;
            } else if (delta > max_d_rpm) {
                max_d_rpm = delta;
            }
            rpm_samples += 1;
            last_rpm = new_rpm;
        }
        elapsed += SHIFT_DELAY_MS;
        vTaskDelay(SHIFT_DELAY_MS/portTICK_RATE_MS);
    }
    if (avg_d_rpm != 0) {
        avg_d_rpm /= rpm_samples;
    }
    this->gear_disagree_count = 0;
    ESP_LOGI("ELAPSE_SHIFT", "SHIFT_END (TO %d) (Actual time %d ms - Target was %d ms). DELTAS: (Max: %d, Min: %d, Avg: %d)", targ_gear, elapsed, s.targ_ms, max_d_rpm, min_d_rpm, avg_d_rpm);
    // Shift complete - Return the elapsed time for the shift to feedback into the adaptation system
    this->tcc->on_shift_complete(this->sensor_data.current_timestamp_ms);
    shift_solenoid->write_pwm_12_bit(0);
    sol_spc->write_pwm_12_bit(0);
    // Fade MPC back
    // Fade MPC back to normal
    this->shifting = false;
    this->flaring = false;
    bool shifted = true;
    if (sensor_data.output_rpm > 200 && this->est_gear_idx == curr_gear) { // WTF Didn't change gears!!!!
        shifted = false;
    }
    return ShiftResponse {
        .shifted = shifted,
        .valid_measurement = shift_measure_complete,
        .time_ms = (int)elapsed,
        .avg_d_rpm = avg_d_rpm,
        .max_d_rpm = max_d_rpm,
        .min_d_rpm = min_d_rpm,
    };
}

void Gearbox::shift_thread() {
    this->shifting = true;
    GearboxGear curr_target = this->target_gear;
    GearboxGear curr_actual = this->actual_gear;
    uint16_t batt_voltage = 12000; // Assume 12V
    Sensors::read_vbatt(&batt_voltage);
    if (curr_actual == curr_target) {
        ESP_LOGW("SHIFTER", "Gears are the same????");
        goto cleanup;
    }
    if (!is_controllable_gear(curr_actual) && !is_controllable_gear(curr_target)) { // N->P or P->N
        sol_mpc->write_pwm_percent_with_voltage(find_mpc_hold_pressure(sensor_data.engine_rpm, sensor_data.atf_temp), sensor_data.voltage);
        sol_spc->write_pwm_percent_with_voltage(500, sensor_data.voltage); // 40%
        sol_y4->write_pwm_percent_with_voltage(500, sensor_data.voltage); // 3-4 is pulsed at 20%
        ESP_LOGI("SHIFTER", "No need to shift");
        this->actual_gear = curr_target; // Set on startup
        goto cleanup;
    } else if (is_controllable_gear(curr_actual) != is_controllable_gear(curr_target)) { // This would be a garage shift, either in or out
        ESP_LOGI("SHIFTER", "Garage shift");
        if (is_controllable_gear(curr_target)) {
            // N/P -> R/D
            int press = find_mpc_hold_pressure(sensor_data.engine_rpm, sensor_data.atf_temp);
            sol_mpc->write_pwm_percent_with_voltage(press, sensor_data.voltage);
            sol_spc->write_pwm_percent_with_voltage(press*0.9, sensor_data.voltage);
            //sol_mpc->write_pwm_percent(50); // Increase MPC pressure to keep B2 clutch in suspension
            sol_y4->write_pwm_percent_with_voltage(1000, sensor_data.voltage); // Full on
            // Slowly ramp up SPC pressure again
            vTaskDelay(1500);
            sol_y4->write_pwm_percent(0);
            sol_spc->write_pwm_percent(0);
            sol_mpc->write_pwm_percent_with_voltage(find_mpc_hold_pressure(sensor_data.engine_rpm, sensor_data.atf_temp), sensor_data.voltage);
        } else {
            // Garage shifting to N or P, we can just set the pressure back to idle
            sol_spc->write_pwm_percent_with_voltage(500, sensor_data.voltage);
            sol_mpc->write_pwm_percent_with_voltage(find_mpc_hold_pressure(sensor_data.engine_rpm, sensor_data.atf_temp), sensor_data.voltage);
            sol_y4->write_pwm_percent_with_voltage(500, sensor_data.voltage); // Back to idle
        }
        this->actual_gear = curr_target; // and we are in gear!
        goto cleanup;
    } else { // Both gears are controllable
        ESP_LOGI("SHIFTER", "Both gears are controllable");
        if (is_fwd_gear(curr_target) != is_fwd_gear(curr_actual)) {
            // In this case, we set the current gear to neutral, then thread will re-spawn
            ESP_LOGI("SHIFTER", "Shifter got stuck in R-D. Returning and trying again");
            this->actual_gear = GearboxGear::Neutral;
            goto cleanup;
        } else if (is_fwd_gear(curr_target) && is_fwd_gear(curr_actual)){
            // Forward shift logic
            if (curr_target > curr_actual) { // Upshifting
                ESP_LOGI("SHIFTER", "Upshift request to change between %s and %s!", gear_to_text(curr_actual), gear_to_text(curr_target));
                //this->show_upshift = true;
                ProfileGearChange pgc;
                ShiftResponse response;
                Solenoid* sol;
                uint8_t cur_g, tar_g;
                SensorData before = this->sensor_data;
                if (curr_target == GearboxGear::Second) { // 1-2
                    pgc = ProfileGearChange::ONE_TWO;
                    cur_g = 1; tar_g = 2;
                    sol = sol_y3;
                } else if (curr_target == GearboxGear::Third) { // 2-3
                    pgc = ProfileGearChange::TWO_THREE;
                    cur_g = 2; tar_g = 3;
                    sol = sol_y5;
                } else if (curr_target == GearboxGear::Fourth) { // 3-4
                    pgc = ProfileGearChange::THREE_FOUR;
                    cur_g = 3; tar_g = 4;
                    sol = sol_y4;
                } else if (curr_target == GearboxGear::Fifth) { // 4-5
                    pgc = ProfileGearChange::FOUR_FIVE;
                    cur_g = 4; tar_g = 5;
                    sol = sol_y3;
                } else { // WTF
                    this->target_gear = this->actual_gear;
                    goto cleanup;
                }
                portENTER_CRITICAL(&this->profile_mutex);
                AbstractProfile* prof = this->current_profile;
                portEXIT_CRITICAL(&this->profile_mutex);
                response = elapse_shift(pgc, prof, sol, cur_g, tar_g);
                if (response.time_ms != 0xFFFF) {
                    egs_can_hal->set_last_shift_time(response.time_ms);
                    this->actual_gear = curr_target;
                    this->start_second = true;
                    if (prof == standard) {
                        ((StandardProfile*)prof)->on_upshift_complete(response, cur_g, &before);
                    }
                    
                } else { // We didn't change gears!
                    egs_can_hal->set_last_shift_time(0xFFFF);
                    this->target_gear = this->actual_gear;
                }
                goto cleanup;
            } else { // Downshifting
                ESP_LOGI("SHIFTER", "Downshift request to change between %s and %s!", gear_to_text(curr_actual), gear_to_text(curr_target));
                //this->show_downshift = true;
                ProfileGearChange pgc;
                ShiftResponse resp;
                Solenoid* sol;
                uint8_t cur_g, tar_g;
                if (curr_target == GearboxGear::First) { // 2-1
                    pgc = ProfileGearChange::TWO_ONE;
                    cur_g = 2; tar_g = 1;
                    sol = sol_y3;
                    this->start_second = false;
                } else if (curr_target == GearboxGear::Second) { // 3-2
                    pgc = ProfileGearChange::THREE_TWO;
                    cur_g = 3; tar_g = 2;
                    sol = sol_y5;
                    this->start_second = true;
                } else if (curr_target == GearboxGear::Third) { // 4-3
                    pgc = ProfileGearChange::FOUR_THREE;
                    cur_g = 4; tar_g = 3;
                    sol = sol_y4;
                } else if (curr_target == GearboxGear::Fourth) { // 5-4
                    pgc = ProfileGearChange::FIVE_FOUR;
                    cur_g = 5; tar_g = 4;
                    sol = sol_y3;
                } else { // WTF
                    this->target_gear = this->actual_gear;
                    goto cleanup;
                }
                portENTER_CRITICAL(&this->profile_mutex);
                AbstractProfile* prof = this->current_profile;
                portEXIT_CRITICAL(&this->profile_mutex);
                resp = elapse_shift(pgc, prof, sol, cur_g, tar_g);
                if (resp.time_ms != 0xFFFF) {
                    egs_can_hal->set_last_shift_time(resp.time_ms);
                    this->actual_gear = curr_target;
                }
                else { // We didn't change gears!
                    egs_can_hal->set_last_shift_time(0xFFFF);
                    this->target_gear = this->actual_gear;
                }
                goto cleanup;
            }
        } else {
            ESP_LOGI("SHIFTER", "Ignoring request to change between %s and %s!", gear_to_text(curr_actual), gear_to_text(curr_target));
        }
        goto cleanup;
    }
cleanup:
    ESP_LOGI("SHIFTER", "Shift complete");
    egs_can_hal->set_torque_request(TorqueRequest::None);
    egs_can_hal->set_requested_torque(0);
    this->shifting = false;
    //this->show_downshift = false;
    //this->show_upshift = false;
    vTaskDelete(nullptr);
}

void Gearbox::inc_subprofile() {
    portENTER_CRITICAL(&this->profile_mutex);
    if (this->current_profile != nullptr) {
        this->current_profile->increment_subprofile();
    }
    portEXIT_CRITICAL(&this->profile_mutex);
}

void Gearbox::controller_loop() {
    bool lock_state = false;
    ShifterPosition last_position = ShifterPosition::SignalNotAvaliable;
    // Before we enter, we have to check what gear we are in as the 'actual gear'
    ESP_LOGI("GEARBOX", "GEARBOX START!");
    while(1) {
        uint64_t now = esp_timer_get_time()/1000;
        this->sensor_data.current_timestamp_ms = now;
        bool can_read = true;
        if (!this->calc_input_rpm(&sensor_data.input_rpm)) {
            can_read = false;
        }
        
        if(can_read && this->calc_output_rpm(&this->sensor_data.output_rpm, now)) {
            if (!shifting && this->sensor_data.output_rpm > 500 && this->sensor_data.input_rpm > 500) {
                if (is_fwd_gear(this->actual_gear)) {
                    if (calcGearFromRatio(false) && this->est_gear_idx != 0) {
                        // Compare gears
                        GearboxGear estimate = gear_from_idx(this->est_gear_idx);
                        if (estimate != this->actual_gear) {
                            gear_disagree_count++;
                            if (gear_disagree_count >= 50) {
                                this->actual_gear = estimate; // DID NOT SHIFT!
                                this->target_gear = estimate;
                            }
                        } else {
                            gear_disagree_count = 0;
                        }
                    }
                } else {
                    gear_disagree_count = 0;
                }
            } else {
                gear_disagree_count = 0;
            }
        } else {
            can_read = false;
            gear_disagree_count = 0;
        }
        
        egs_can_hal->set_input_shaft_speed(this->sensor_data.input_rpm);
        if (can_read && this->sensor_data.output_rpm >= 100) {
            bool rev = !is_fwd_gear(this->target_gear);
            if (!this->calcGearFromRatio(rev)) {
                //ESP_LOGE("GEARBOX", "GEAR RATIO IMPLAUSIBLE");
            }
        }
        sensor_data.is_braking = egs_can_hal->get_is_brake_pressed(now, 250);
        this->sensor_data.engine_rpm = egs_can_hal->get_engine_rpm(now, 250);
        if (this->sensor_data.engine_rpm == UINT16_MAX) {
            this->sensor_data.engine_rpm = 0;
        }
        if (Sensors::parking_lock_engaged(&lock_state)) {
            egs_can_hal->set_safe_start(lock_state);
            ShifterPosition pos = egs_can_hal->get_shifter_position_ewm(now, 250);
            if (
                pos == ShifterPosition::P || // Only obide by definitive positions for now, no intermittent once
                pos == ShifterPosition::R ||
                pos == ShifterPosition::N ||
                pos == ShifterPosition::D
            ) {
                if (pos != last_position) {
                    if (lock_state) {
                        if (pos == ShifterPosition::P) {
                            this->target_gear = GearboxGear::Park;
                            last_position = pos;
                            if (this->tcc != nullptr) {
                                this->tcc->save_adaptation_data();
                            }
                        } else if (pos == ShifterPosition::N) {
                            this->target_gear = GearboxGear::Neutral;
                            last_position = pos;
                        }
                    } else {
                        // Drive or R!
                        if (pos == ShifterPosition::R) {
                            this->target_gear = this->start_second ? GearboxGear::Reverse_Second : GearboxGear::Reverse_First;
                            last_position = pos;
                        } else if (pos == ShifterPosition::D) {
                            this->target_gear = this->start_second ? GearboxGear::Second : GearboxGear::First;
                            last_position = pos;
                        }
                    }
                }
            }
        }
        if (this->sensor_data.engine_rpm > 500) {
            if (!shifting && control_solenoids) {
                sol_mpc->write_pwm_percent_with_voltage(find_mpc_hold_pressure(sensor_data.engine_rpm, sensor_data.atf_temp), sensor_data.voltage);
            }
            if (is_fwd_gear(this->actual_gear)) {
                bool want_upshift = false;
                bool want_downshift = false;
                if (!shifting && this->actual_gear == this->target_gear) {
                    // Enter critical ISR section
                    portENTER_CRITICAL(&this->profile_mutex);
                    AbstractProfile* p = this->current_profile;
                    // Exit critical
                    portEXIT_CRITICAL(&this->profile_mutex);
                    // Check if profile is loaded
                    if (p != nullptr) {
                        // Ask the current drive profile if it thinks, given the current
                        // data, if the car should up/downshift
                        if(p->should_upshift(this->actual_gear, &this->sensor_data)) {
                            want_upshift = true;
                        }
                        if(p->should_downshift(this->actual_gear, &this->sensor_data)) {
                            want_downshift = true;
                        }
                    }
                    if (want_upshift && want_downshift) {
                        egs_can_hal->set_display_msg(GearboxMessage::ActuateParkingBreak); // WTF
                    } else if ((this->ask_upshift || want_upshift) && this->actual_gear < GearboxGear::Fifth) {
                        // Check RPMs
                        GearboxGear next = next_gear(this->actual_gear);
                        if (calc_input_rpm_from_req_gear(this->sensor_data.output_rpm, next) > STALL_RPM+100) {
                            this->target_gear = next;
                            this->sensor_data.last_shift_time = now;
                        }
                    } else if ((this->ask_downshift || want_downshift) && this->actual_gear > GearboxGear::First) {
                        // Check RPMs
                        GearboxGear prev = prev_gear(this->actual_gear);
                        if (calc_input_rpm_from_req_gear(this->sensor_data.output_rpm, prev) < REDLINE_RPM-400) {
                            this->target_gear = prev;
                            this->sensor_data.last_shift_time = now;
                        }
                    }
                    //this->ask_downshift = false;
                    //this->ask_upshift = false;
                }
                // Now, how do we prioritize up/downshift behaviour?
                // Upshifting when accelerating should take precidence.

                if (this->ask_upshift) {
                    this->ask_upshift = false;
                    if (this->actual_gear < GearboxGear::Fifth && this->target_gear == this->actual_gear && !shifting) {
                        this->target_gear = next_gear(this->actual_gear);
                    }
                } else if (this->ask_downshift) {
                    this->ask_downshift = false;
                    if (this->actual_gear > GearboxGear::First && this->target_gear == this->actual_gear && !shifting) {
                        this->target_gear = prev_gear(this->actual_gear);
                    }
                }
                if (can_read) {
                    uint8_t p_tmp = egs_can_hal->get_pedal_value(now, 100);
                    if (p_tmp != 0xFF) {
                        this->sensor_data.pedal_pos = p_tmp;
                    }
                    if (!Sensors::read_vbatt(&this->sensor_data.voltage)) {
                        this->sensor_data.voltage = 12000;
                    }

                    if (is_fwd_gear(this->actual_gear) && is_fwd_gear(this->target_gear)) {
                        this->sensor_data.tcc_slip_rpm = sensor_data.engine_rpm - sensor_data.input_rpm;
                        if (this->tcc != nullptr) {
                            this->tcc->update(this->target_gear, LockupType::Shut, &this->sensor_data, this->shifting);
                        }
                    } else {
                        this->tcc_percent = 0;
                        sol_tcc->write_pwm_12_bit(0);
                    }
                }
            }
            if (this->target_gear != this->actual_gear && this->shifting == false) {
                // Create shift task to change gears for us!
                xTaskCreatePinnedToCore(Gearbox::start_shift_thread, "Shift handler", 8192, this, 10, &this->shift_task, 1);
            }
        } else {
            if (control_solenoids) {
                sol_mpc->write_pwm_12_bit(0);
                sol_spc->write_pwm_12_bit(0);
                sol_tcc->write_pwm_12_bit(0);
                sol_y3->write_pwm_12_bit(0);
                sol_y4->write_pwm_12_bit(0);
                sol_y5->write_pwm_12_bit(0);
            } else {
                egs_can_hal->set_safe_start(false); // Inhibit engine starting when solenoids are not controlled!
            }
        }
        int tmp_atf = 0;
        if (!Sensors::read_atf_temp(&tmp_atf)) {
            // Default to engine coolant
            this->sensor_data.atf_temp = (egs_can_hal->get_engine_coolant_temp(now, 250));
        } else {
            this->sensor_data.atf_temp = tmp_atf;
        }
        egs_can_hal->set_gearbox_temperature(this->sensor_data.atf_temp);
        egs_can_hal->set_shifter_position(egs_can_hal->get_shifter_position_ewm(now, 250));

        egs_can_hal->set_target_gear(this->target_gear);
        egs_can_hal->set_actual_gear(this->actual_gear);
        egs_can_hal->set_solenoid_pwm(sol_y3->get_pwm() >> 4, SolenoidName::Y3);
        egs_can_hal->set_solenoid_pwm(sol_y4->get_pwm() >> 4, SolenoidName::Y4);
        egs_can_hal->set_solenoid_pwm(sol_y5->get_pwm() >> 4, SolenoidName::Y5);
        egs_can_hal->set_solenoid_pwm(sol_spc->get_pwm() >> 4, SolenoidName::SPC);
        egs_can_hal->set_solenoid_pwm(sol_mpc->get_pwm() >> 4, SolenoidName::MPC);
        egs_can_hal->set_solenoid_pwm(sol_tcc->get_pwm() >> 4, SolenoidName::TCC);

        int static_torque = egs_can_hal->get_static_engine_torque(now, 500);
        if (static_torque != INT_MAX) {
            this->sensor_data.static_torque = static_torque;
        }
        int max_torque = egs_can_hal->get_maximum_engine_torque(now, 500);
        if (max_torque != INT_MAX) {
            this->sensor_data.max_torque = max_torque;
        }
        int min_torque = egs_can_hal->get_minimum_engine_torque(now, 500);
        if (min_torque != INT_MAX) {
            this->sensor_data.min_torque = min_torque;
        }

        //ESP_LOGI("GEARBOX", "Torque: MIN: %3d, MAX: %3d, STAT: %3d", min_torque, max_torque, static_torque);
        // Show debug symbols on IC
        if (this->show_upshift && this->show_downshift) {
            egs_can_hal->set_display_msg(GearboxMessage::RequestGearAgain);
        } else if (this->show_upshift) {
            egs_can_hal->set_display_msg(GearboxMessage::Upshift); 
        } else if (this->show_downshift) {
            egs_can_hal->set_display_msg(GearboxMessage::Downshift);
        } else {
            egs_can_hal->set_display_msg(GearboxMessage::None);
        }
        if (is_fwd_gear(this->actual_gear)) {
            if (sensor_data.input_rpm > 1000) {
                float percent = (float)sensor_data.engine_rpm/(float)sensor_data.input_rpm;
                uint16_t innertia = TCC_INTERTIA_NM*percent;
                if (innertia > 0x3C) {
                    innertia = 0x3C;
                }
                egs_can_hal->set_turbine_torque_loss(innertia);
            } else {
                egs_can_hal->set_turbine_torque_loss(0);
            }
        } else {
            egs_can_hal->set_turbine_torque_loss(0);
        }

        // Lastly, set display gear
        portENTER_CRITICAL(&this->profile_mutex);
        if (this->current_profile != nullptr) {
            if (this->flaring) {
                // Takes president
                egs_can_hal->set_display_msg(GearboxMessage::None);
                egs_can_hal->set_display_gear(GearboxDisplayGear::Failure, false);
            } else {
                egs_can_hal->set_display_gear(this->current_profile->get_display_gear(this->target_gear, this->actual_gear), this->current_profile == manual);
            }
        }
        portEXIT_CRITICAL(&this->profile_mutex);
        vTaskDelay(20/portTICK_RATE_MS); // 50 updates/sec!
    }
}

bool Gearbox::calc_input_rpm(int* dest) {
    uint32_t n2 = Sensors::read_n2_rpm();
    if (n2 < 50) { // Skip erroneous pulses
        n2 = 0;
    }
    uint32_t n3 = Sensors::read_n3_rpm();
    if (n3 < 50) { // Skip erroneous pulses
        n3 = 0;
    }
    // Compare N2 and N3 sensors based on our TARGET gear
    if (this->actual_gear == GearboxGear::Neutral || this->actual_gear == GearboxGear::Park) { 
        if (n3 < 100 && n2 != 0) {
            *dest = n2 * 1.64;
            return true;
        } else {
            *dest = (n2+n3)/2;
            return true;
        }
    }
    // Handle forward gear transitional RPMs
    // Prevents torque converter code panicing during 1->2 and 5->4 gear transition,
    // as it would falsely assume the gearbox has suddenly dropped to 0 RPM!
    if (
        (this->target_gear == GearboxGear::Fifth  && this->actual_gear == GearboxGear::Fourth) || // 4->5 up
        (this->target_gear == GearboxGear::Fourth && this->actual_gear == GearboxGear::Fifth)  || // 5->4 dn
        (this->target_gear == GearboxGear::First  && this->actual_gear == GearboxGear::Second) || // 2->1 up
        (this->target_gear == GearboxGear::Second && this->actual_gear == GearboxGear::First)     // 1->2 dn
    ) {
        if (abs((int)n3-(int)n2) > 100) {
            *dest = n2 * 1.64;
            return true;
        } else {
            *dest =  (n3+n2)/2;
            return true;
        }
    }

    switch (this->target_gear) {
        case GearboxGear::First:
        case GearboxGear::Fifth:
            n2 *= 1.64;
            if (n2 > OVERSPEED_RPM) {
                //ESP_LOGE("CALC_INPUT_RPM", "N2 overspeed detected!");
                return false;
            } else {
                *dest = n2;
                return true;
            }
        case GearboxGear::Reverse_First:
        case GearboxGear::Reverse_Second:
            if (n3 > OVERSPEED_RPM) {
                //ESP_LOGE("CALC_INPUT_RPM", "N3 overspeed detected!");
                return false;
            } else {
                *dest = n3;
                return true;
            }
        default:
            // Compare both!
            if (n2 > OVERSPEED_RPM || n3 > OVERSPEED_RPM) {
                //ESP_LOGE("CALC_INPUT_RPM", "N2 or N3 overspeed detected!");
                return false;
            }
            // Rational check
            if (this->target_gear == this->actual_gear) { // Only perform rational check if we are defiantly in the right gear!
                if (n3 > n2) {
                    if (n3-n2 > 250) { // Rational check
                        //ESP_LOGE("CALC_INPUT_RPM", "Rational check failed! N3-N2 delta is %d", n3-n2);
                        return false;
                    }
                } else if (n2 > n3) {
                    if (n2-n3 > 250) { // Rational check
                        //ESP_LOGE("CALC_INPUT_RPM", "Rational check failed! N2-N3 delta is %d", n2-n3);
                        return false;
                    }
                }
            }
            *dest = (n2+n3)/2;
            return true;
    }
}

bool Gearbox::calc_output_rpm(int* dest, uint64_t now) {
    WheelData left = egs_can_hal->get_rear_left_wheel(now, 250);
    WheelData right = egs_can_hal->get_rear_right_wheel(now, 250);
    //ESP_LOGI("WRPM","R:(%d %d) L:(%d %d)", (int)right.current_dir, right.double_rpm, (int)left.current_dir, left.double_rpm);
    float rpm = 0;
    if (left.current_dir == WheelDirection::SignalNotAvaliable && right.current_dir == WheelDirection::SignalNotAvaliable) {
        //ESP_LOGE("CALC_OUTPUT_RPM", "Could not obtain right and left wheel RPM!");
        return false;
    } else if (left.current_dir == WheelDirection::SignalNotAvaliable) { // Right OK
        ESP_LOGW("CALC_OUTPUT_RPM", "Could not obtain left wheel RPM, trusting the right one!");
        rpm = right.double_rpm;
    } else if (right.current_dir == WheelDirection::SignalNotAvaliable) { // Left OK
        ESP_LOGW("CALC_OUTPUT_RPM", "Could not obtain right wheel RPM, trusting the left one!");
        rpm = left.double_rpm;
    } else { // Both sensors OK!
        rpm = abs(left.double_rpm) > abs(right.double_rpm) ? left.double_rpm : right.double_rpm;
    }
    rpm *= diff_ratio_f;
    rpm /= 2;
    *dest = rpm;
    return true;
}

bool Gearbox::calcGearFromRatio(bool is_reverse) {
    float ratio = (float)this->sensor_data.input_rpm / (float)this->sensor_data.output_rpm;
    if (is_reverse) {
        ratio *=-1;
        for(uint8_t i = 0; i < 2; i++) { // Scan the 2 reverse gears
            GearRatioLimit limits = GEAR_RATIO_LIMITS[i+5];
            if (ratio >= limits.min && ratio <= limits.max) {
                this->est_gear_idx = i+1;
                return true;
            }
        }
    } else {
        for(uint8_t i = 0; i < 5; i++) { // Scan the 5 forwards gears
            GearRatioLimit limits = GEAR_RATIO_LIMITS[i];
            if (ratio > limits.min && ratio < limits.max) {
                this->est_gear_idx = i+1;
                return true;
            }
        }
    }
    this->est_gear_idx = 0;
    return false;
}