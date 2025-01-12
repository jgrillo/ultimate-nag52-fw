#ifndef __PRESSURE_MANAGER_H_
#define __PRESSURE_MANAGER_H_

#include <common_structs.h>
#include "tcm_maths.h"
#include "profiles.h"
#include "adaptation/adapt_map.h"
#include <gearbox_config.h>
#include "nvs/eeprom_config.h"

// Shift phase IDs

#define SHIFT_PHASE_BLEED 1
#define SHIFT_PHASE_FILL 2
#define SHIFT_PHASE_TORQUE 3
#define SHIFT_PHASE_OVERLAP 4
#define SHIFT_PHASE_MAX_P 5

enum class Clutch {
    K1,
    K2,
    K3,
    B1,
    B2,
    B3 // Reverse ONLY
};

// 0 -> 100% rated torque (In mbar) (0Nm - 330Nm)
const pressure_map working_norm_pressure_p_small = { 500,  600,  700,  800,  900, 1000, 1100, 1200, 1300, 1400, 1500}; // Working pressure table Park or N
const pressure_map working_norm_pressure_r_small = { 750,  800, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000}; // Working pressure table R1 or R2
const pressure_map working_norm_pressure_1_small = { 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250}; // Working pressure table 1
const pressure_map working_norm_pressure_2_small = { 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250}; // Working pressure table 2
const pressure_map working_norm_pressure_3_small = { 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250}; // Working pressure table 3
const pressure_map working_norm_pressure_4_small = { 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250}; // Working pressure table 4
const pressure_map working_norm_pressure_5_small = { 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250}; // Working pressure table 5

// 0 -> 100% rated torque (In mbar) (0Nm - 580Nm)
const pressure_map working_norm_pressure_p_large = {  750, 1000, 1300, 1600, 1900, 2100, 2400, 2700, 3000, 3300, 3600}; // Working pressure table R1 or R2
const pressure_map working_norm_pressure_r_large = {  750, 1000, 1300, 1600, 1900, 2100, 2400, 2700, 3000, 3300, 3600}; // Working pressure table R1 or R2
const pressure_map working_norm_pressure_1_large = {  900, 1100, 1200, 1500, 1800, 2100, 2400, 2700, 3000, 3300, 3600}; // Working pressure table 1
const pressure_map working_norm_pressure_2_large = {  900, 1100, 1200, 1500, 1800, 2100, 2400, 2700, 3000, 3300, 3600}; // Working pressure table 2
const pressure_map working_norm_pressure_3_large = {  900, 1100, 1200, 1500, 1800, 2300, 2600, 2900, 3200, 3500, 3800}; // Working pressure table 3
const pressure_map working_norm_pressure_4_large = {  900, 1100, 1200, 1500, 1800, 2300, 2600, 2900, 3200, 3500, 3800}; // Working pressure table 4
const pressure_map working_norm_pressure_5_large = {  900, 1100, 1200, 1500, 1800, 2300, 2600, 2900, 3200, 3500, 3800}; // Working pressure table 5

typedef void (*P_RAMP_FUNC)(float, float);

class PressureManager {

public:

    /**
     * @brief Set the target MPC pressure (Modulating pressure)
     * 
     * @param targ Target MPC pressure to achieve in mBar
     */
    void set_target_mpc_pressure(uint16_t targ);

    /**
     * @brief Set the target SPC pressure (Shift pressure)
     * 
     * @param targ Target SPC pressure to achieve in mBar
     */
    void set_target_spc_pressure(uint16_t targ);

    /**
     * @brief Set the target TCC pressure (Torque converter)
     * 
     * @param targ Target TCC pressure to achieve in mBar
     */
    void set_target_tcc_pressure(uint16_t targ);
    uint16_t get_targ_mpc_pressure();
    uint16_t get_targ_spc_pressure();
    uint16_t get_targ_tcc_pressure();
    uint16_t get_targ_spc_current();
    uint16_t get_targ_mpc_current();
    void disable_spc();

    PressureManager(SensorData* sensor_ptr, uint16_t max_torque);

    /**
     * @brief Get the shift data object for the requested gear change
     * 
     * @param shift_request Which gear shift is being requested
     * @param chars Shift requested characteristics
     * @param curr_mpc Current MPC working pressure at the time of shift
     * @return ShiftData 
     */
    ShiftData get_shift_data(ProfileGearChange shift_request, ShiftCharacteristics chars, uint16_t curr_mpc);

    /**
     * @brief Reset adaptation data
     * 
     * @return true if adaptation reset was OK
     * @return false if adaptation reset failed
     */
    bool diag_reset_adaptation() {
        if (this->adapt_map != nullptr) { 
            this->adapt_map->reset();
            return true;
        } else {
            return false;
        }
    }

    /**
     * @brief Called after every gear change to try and better improve future shifts
     * 
     * @param prefill_sensors Sensor data from just before the fill stage
     * @param change Gear change executed
     * @param response The report of the gear change
     * @param is_valid_rpt If the response is valid or not (Invalid would be due to a shift at stantstill)
     */
    void perform_adaptation(SensorData* prefill_sensors, ProfileGearChange change, ShiftReport* response, bool is_valid_rpt) {
        if (this->adapt_map != nullptr) { 
            this->adapt_map->perform_adaptation(prefill_sensors, response, change, is_valid_rpt, this->gb_max_torque);
        }
    }

    /**
     * @brief Save adaptation data to NVS EEPROM
     * 
     */
    void save() {
        if (this->adapt_map != nullptr) { 
            this->adapt_map->save(); 
        }
    }

    uint16_t find_working_mpc_pressure(GearboxGear curr_g, int max_rated_torque);
    
    float get_tcc_temp_multiplier(int atf_temp);

    void make_fill_data(ShiftPhase* dest, ShiftCharacteristics chars, ProfileGearChange change, uint16_t curr_mpc);
    void make_torque_data(ShiftPhase* dest, ShiftPhase* prev, ShiftCharacteristics chars, ProfileGearChange change, uint16_t curr_mpc);
    void make_overlap_data(ShiftPhase* dest, ShiftPhase* prev, ShiftCharacteristics chars, ProfileGearChange change, uint16_t curr_mpc);
    void make_max_p_data(ShiftPhase* dest, ShiftPhase* prev, ShiftCharacteristics chars, ProfileGearChange change, uint16_t curr_mpc);
private:
     /**
     * Returns the estimated PWM to send to either SPC or MPC solenoid
     * Based on the requested pressure that is needed withint either pressure rail.
     */
    uint16_t get_p_solenoid_current(uint16_t request_mbar, bool is_spc);

    /**
     * Returns the estimated PWM to send to the TCC solenoid
     * based on the requested pressure that is needed
     */
    uint16_t get_tcc_solenoid_pwm_duty(uint16_t request_mbar);

    SensorData* sensor_data;
    AdaptationMap* adapt_map;

    uint16_t req_tcc_pressure;
    uint16_t req_spc_pressure;
    uint16_t req_mpc_pressure;
    uint16_t req_current_spc;
    uint16_t req_current_mpc;
    TcmMap* pressure_pwm_map;
    TcmMap* tcc_pwm_map;
    TcmMap* hold2_time_map;
    uint16_t gb_max_torque;


    Clutch get_clutch_to_release(ProfileGearChange change);
    Clutch get_clutch_to_apply(ProfileGearChange change);

};

#endif