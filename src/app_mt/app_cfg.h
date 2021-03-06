
#ifndef APP_CFG_H
#define APP_CFG_H

#include "types.h"
#include "temp_control.h"
#include "temp_profile.h"
#include "touch_calib.h"
#include "web_api.h"
#include "net.h"
#include "fault.h"
#include "offset.h"
#include "sensor.h"
#include "ota_update.h"


typedef enum {
  SS_DEVICE,
  SS_SERVER
} settings_source_t;


void
app_cfg_init(void);

void
app_cfg_reset(void);

unit_t
app_cfg_get_temp_unit(void);

void
app_cfg_set_temp_unit(unit_t temp_unit);

output_ctrl_t
app_cfg_get_control_mode(void);

void
app_cfg_set_control_mode(output_ctrl_t control_mode);

quantity_t
app_cfg_get_hysteresis(void);

void
app_cfg_set_hysteresis(quantity_t hysteresis);

quantity_t
app_cfg_get_screen_saver(void);

void
app_cfg_set_screen_saver(quantity_t screen_saver);

quantity_t
app_cfg_get_probe_offset(sensor_serial_t sensor_serial);

void
app_cfg_set_probe_offset(quantity_t probe_offset, sensor_serial_t sensor_serial);

const matrix_t*
app_cfg_get_touch_calib(void);

void
app_cfg_set_touch_calib(matrix_t* touch_calib);

const controller_settings_t*
app_cfg_get_controller_settings(temp_controller_id_t controller);

void
app_cfg_set_controller_settings(
    temp_controller_id_t controller,
    settings_source_t source,
    controller_settings_t* settings);

const temp_profile_checkpoint_t*
app_cfg_get_temp_profile_checkpoint(temp_controller_id_t controller);

void
app_cfg_set_temp_profile_checkpoint(temp_controller_id_t controller, temp_profile_checkpoint_t* checkpoint);

const output_settings_t*
app_cfg_get_output_settings(output_id_t output);

void
app_cfg_set_output_settings(output_id_t output, output_settings_t* settings);

const char*
app_cfg_get_auth_token(void);

void
app_cfg_set_auth_token(const char* auth_token);

const net_settings_t*
app_cfg_get_net_settings(void);

void
app_cfg_set_net_settings(const net_settings_t* settings);

const ota_update_checkpoint_t*
app_cfg_get_ota_update_checkpoint(void);

void
app_cfg_set_ota_update_checkpoint(const ota_update_checkpoint_t* checkpoint);

uint32_t
app_cfg_get_reset_count(void);

const fault_data_t*
app_cfg_get_fault_data(void);

void
app_cfg_clear_fault_data(void);

void
app_cfg_set_fault_data(fault_type_t fault_type, void* data, uint32_t data_size);

void
app_cfg_flush(void);

#endif
