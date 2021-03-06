/*
 * Copyright (c) 2018 Björn Schmidt
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

/* 
 * File:   ACAcontrollerState.h
 * Author: Björn Schmidt
 *
 * Created on August 30, 2018, 8:03 PM
 */

#include <stdio.h>
#include "config.h"
#include "stm8s.h"
#include "stm8s_itc.h"
#include "stm8s_gpio.h"
#include "gpio.h"
#include "ACAeeprom.h"
#include "ACAcontrollerState.h"
#include "ACAcommons.h"

// user controllable settings
uint8_t ui8_throttle_min_range = 32;
uint8_t ui8_throttle_max_range = 192;
uint8_t ui8_speedlimit_kph = 25; // normal limit
uint8_t ui8_speedlimit_without_pas_kph = 6; // limit without pas activity
uint8_t ui8_speedlimit_with_throttle_override_kph = 35; // limit with pas and throttle both active
uint8_t ui8_speedlimit_actual_kph; // dynamic speedlimit based on current state
float flt_s_pas_threshold = 1.7;
float flt_s_pid_gain_p = 0.5;
float flt_s_pid_gain_i = 0.2;
float flt_torquesensorCalibration = 0.0;
uint16_t ui16_s_ramp_end = 1500;
uint16_t ui16_s_ramp_start = 7000;
uint8_t ui8_s_motor_angle = 214;

// internal
uint32_t uint32_icc_signals = 0;

uint8_t ui8_assistlevel_dynamic_addon = 0;
uint8_t ui8_assistlevel_global = 83; // 3 + max regen
uint8_t PAS_act = 3; //recent PAS direction reading
uint8_t PAS_is_active = 0;
uint16_t ui16_sum_torque = 0; //sum of array elements
uint8_t ui8_offroad_state = 0; //state of offroad switching procedure
uint32_t uint32_current_target = 0; //target for PI-Control
uint16_t ui16_setpoint = 0;
uint16_t ui16_throttle_accumulated = 0;
uint8_t ui8_current_cal_a = 0;
uint16_t ui16_current_cal_b = 0;
uint16_t ui16_x4_cal_b = 0;
uint16_t ui16_throttle_cal_b = 0;
uint16_t ui16_battery_current_max_value = 0;
uint16_t ui16_regen_current_max_value = 0;
uint8_t ui8_motor_state = 0;
uint8_t ui8_BatteryVoltage = 0; //Battery Voltage read from ADC
uint16_t ui16_motor_speed_erps = 0;
uint32_t ui32_erps_filtered = 0; //filtered value of erps
uint16_t ui16_virtual_erps_speed = 0;
uint16_t ui16_BatteryCurrent = 0; //Battery Current read from ADC8
uint8_t ui8_position_correction_value = 127; // in 360/256 degrees
uint16_t ui16_ADC_iq_current = 0;
uint16_t ui16_ADC_iq_current_filtered = 0;
uint8_t ui8_control_state = 0;
uint8_t ui8_uptime = 0;

int8_t i8_motor_temperature = 0;

uint8_t uint8_t_hall_case[7];
uint8_t uint8_t_hall_order[6];
int8_t int8_t_hall_counter = 0;
uint8_t ui8_hall_order_counter = 5;

uint16_t ui16_speed_kph_to_erps_ratio = 0;

uint32_t ui32_SPEED_km_h; //global variable Speed
uint32_t ui32_SPEED_km_h_accumulated;
uint16_t ui16_time_ticks_between_speed_interrupt = 64000L; //speed in timetics
uint16_t ui16_time_ticks_for_speed_calculation = 0; //time tics for speed measurement
uint8_t ui8_SPEED_Flag = 0; //flag for SPEED interrupt
uint8_t ui8_offroad_counter = 0; //counter for offroad switching procedure

uint16_t ui16_aca_flags = 0; //if throttle input should be bases on assist level and other flags

uint8_t ui8_adc_read_throttle_busy = 0;
uint16_t ui16_torque[NUMBER_OF_PAS_MAGS]; //array for torque values of one crank revolution
uint8_t ui8_torque_index = 0; //counter for torque array

uint16_t ui16_time_ticks_between_pas_interrupt_smoothed = 0;
uint16_t ui16_time_ticks_for_pas_calculation = 0; //time tics for cadence measurement
uint16_t ui16_PAS_High_Counter = 1; //time tics for direction detection
uint16_t ui16_time_ticks_between_pas_interrupt = 64000L; //cadence in timetics
uint16_t ui16_PAS_High = 1; //number of High readings on PAS
uint8_t ui8_PAS_update_call_when_inactive_counter = 50; //increased when no pas change is detected (50Hz)
uint8_t ui8_PAS_Flag = 0;

void controllerstate_init(void) {
    uint8_t di;
    uint8_t eepromVal;
    uint8_t eepromHighVal;

    // convert static defines to volatile vars
    ui16_aca_flags = ACA;
    ui8_speedlimit_kph = limit;
    ui8_speedlimit_without_pas_kph = limit_without_pas;
    ui8_speedlimit_with_throttle_override_kph = limit_with_throttle_override;
    ui8_speedlimit_actual_kph = limit;
    ui8_throttle_min_range = ADC_THROTTLE_MIN_VALUE;
    ui8_throttle_max_range = ADC_THROTTLE_MAX_VALUE;
    flt_s_pas_threshold = PAS_THRESHOLD;
    flt_s_pid_gain_p = P_FACTOR;
    flt_s_pid_gain_i = I_FACTOR;
    ui16_s_ramp_start = RAMP_START;
    ui16_s_ramp_end = RAMP_END;
    ui8_s_motor_angle = MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT;
    ui16_battery_current_max_value = BATTERY_CURRENT_MAX_VALUE;
    ui16_regen_current_max_value = REGEN_CURRENT_MAX_VALUE;
    ui8_current_cal_a = current_cal_a;

    // read in overrides from eeprom if they are > 0, assuming 0s are uninitialized
    eepromHighVal = eeprom_read(OFFSET_BATTERY_CURRENT_MAX_VALUE_HIGH_BYTE);
    eepromVal = eeprom_read(OFFSET_BATTERY_CURRENT_MAX_VALUE);
    if (eepromVal > 0 || eepromHighVal > 0) ui16_battery_current_max_value = ((uint16_t) eepromHighVal << 8) + (uint16_t) eepromVal;

    eepromHighVal = eeprom_read(OFFSET_ACA_FLAGS_HIGH_BYTE);
    eepromVal = eeprom_read(OFFSET_ACA_FLAGS);
    if (eepromVal > 0 || eepromHighVal > 0)  ui16_aca_flags = ((uint16_t) eepromHighVal << 8) + (uint16_t) eepromVal;
    
    eepromVal = eeprom_read(OFFSET_REGEN_CURRENT_MAX_VALUE);
    if (eepromVal > 0) ui16_regen_current_max_value = eepromVal;
    eepromVal = eeprom_read(OFFSET_MAX_SPEED_DEFAULT);
    if (eepromVal > 0) ui8_speedlimit_kph = eepromVal;
    eepromVal = eeprom_read(OFFSET_MAX_SPEED_WITHOUT_PAS);
    if (eepromVal > 0) ui8_speedlimit_without_pas_kph = eepromVal;
    eepromVal = eeprom_read(OFFSET_MAX_SPEED_WITH_THROTTLE_OVERRIDE);
    if (eepromVal > 0) ui8_speedlimit_with_throttle_override_kph = eepromVal;
    eepromVal = eeprom_read(OFFSET_CURRENT_CAL_A);
    if (eepromVal > 0) ui8_current_cal_a = eepromVal;
    eepromVal = eeprom_read(OFFSET_ASSIST_LEVEL);
    if (eepromVal > 0) ui8_assistlevel_global = eepromVal;
    eepromVal = eeprom_read(OFFSET_THROTTLE_MIN_RANGE);
    if (eepromVal > 0) ui8_throttle_min_range = eepromVal;
    eepromVal = eeprom_read(OFFSET_THROTTLE_MAX_RANGE);
    if (eepromVal > 0) ui8_throttle_max_range = eepromVal;
    eepromVal = eeprom_read(OFFSET_PAS_TRESHOLD);
    if (eepromVal > 0) flt_s_pas_threshold = int2float(eepromVal, 4.0);
    eepromVal = eeprom_read(OFFSET_PID_GAIN_P);
    if (eepromVal > 0) flt_s_pid_gain_p = int2float(eepromVal, 2.0);
    eepromVal = eeprom_read(OFFSET_PID_GAIN_I);
    if (eepromVal > 0) flt_s_pid_gain_i = int2float(eepromVal, 2.0);
    eepromVal = eeprom_read(OFFSET_RAMP_END);
    if (eepromVal > 0) ui16_s_ramp_end = eepromVal << 5;
    eepromVal = eeprom_read(OFFSET_RAMP_START);
    if (eepromVal > 0) ui16_s_ramp_start = eepromVal << 6;
    eepromVal = eeprom_read(OFFSET_MOTOR_ANGLE);
    if (eepromVal > 0) ui8_s_motor_angle = eepromVal;

    for (di = 0; di < 6; di++) {
        uint8_t_hall_order[di] = 0;
    }
    
}

