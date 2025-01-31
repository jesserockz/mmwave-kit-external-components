#include "esphome/core/log.h"
#include "mr24hpc1.h"

#include <utility>
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace mr24hpc1 {

static const char *TAG = "mr24hpc1";

// Calculate CRC check digit
static uint8_t get_frame_crc_sum(uint8_t *data, int len)
{
    unsigned int crc_sum = 0;
    for (int i = 0; i < len - 3; i++)
    {
        crc_sum += data[i];
    }
    return crc_sum & 0xff;
}

// Check that the check digit is correct
static int get_frame_check_status(uint8_t *data, int len)
{
    uint8_t crc_sum = get_frame_crc_sum(data, len);
    uint8_t verified = data[len - 3];
    return (verified == crc_sum) ? 1 : 0;
}

// Print data frame
static void show_frame_data(uint8_t *data, int len)
{
    printf("[%s] FRAME: %d, ", __FUNCTION__, len);
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", data[i] & 0xff);
    }
    printf("\r\n");
}

// Prints the component's configuration data. dump_config() prints all of the component's configuration items in an easy-to-read format, including the configuration key-value pairs.
void mr24hpc1Component::dump_config() { 
    ESP_LOGCONFIG(TAG, "MR24HPC1:");
#ifdef USE_TEXT_SENSOR
    LOG_TEXT_SENSOR("  ", "HeartbeatTextSensor", this->heartbeat_state_text_sensor_);
    LOG_TEXT_SENSOR(" ", "ProductModelTextSensor", this->product_model_text_sensor_);
    LOG_TEXT_SENSOR(" ", "ProductIDTextSensor", this->product_id_text_sensor_);
    LOG_TEXT_SENSOR(" ", "HardwareModelTextSensor", this->hardware_model_text_sensor_);
    LOG_TEXT_SENSOR(" ", "FirwareVerisonTextSensor", this->firware_version_text_sensor_);
    LOG_TEXT_SENSOR(" ", "KeepAwaySensor", this->keep_away_text_sensor_);
    LOG_TEXT_SENSOR(" ", "MotionStatusSensor", this->motion_status_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
    LOG_BINARY_SENSOR(" ", "SomeoneExistsBinarySensor", this->someoneExists_binary_sensor_);
#endif
#ifdef USE_SENSOR
    LOG_SENSOR(" ", "CustomPresenceOfDetectionSensor", this->custom_presence_of_detection_sensor_);
    LOG_SENSOR(" ", "movementsigns", this->movementSigns_sensor_);
    LOG_SENSOR(" ", "custommotiondistance", this->custom_motion_distance_sensor_);
    LOG_SENSOR(" ", "customspatialstaticvalue", this->custom_spatial_static_value_sensor_);
    LOG_SENSOR(" ", "customspatialmotionvalue", this->custom_spatial_motion_value_sensor_);
    LOG_SENSOR(" ", "custommotionspeed", this->custom_motion_speed_sensor_);
#endif
#ifdef USE_SWITCH
    LOG_SWITCH(" ", "underly_open_function", this->underly_open_function_switch_);
#endif
#ifdef USE_BUTTON
    LOG_BUTTON(" ", "ResetButton", this->reset_button_);
#endif
#ifdef USE_SELECT
    LOG_SELECT(" ", "SceneModeSelect", this->scene_mode_select_);
#endif
}

// Initialisation functions
void mr24hpc1Component::setup() {
    s_power_on_status = 0;
    sg_init_flag = true;
    ESP_LOGCONFIG(TAG, "uart_settings is 115200");
    this->check_uart_settings(115200);

    memset(this->c_product_mode, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_product_id, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_firmware_version, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_hardware_model, 0, PRODUCT_BUF_MAX_SIZE);
}

// component callback function, which is called every time the loop is called
void mr24hpc1Component::update() {
    if (!sg_init_flag)                // The setup function is complete.
        return;
    if (sg_init_flag && (255 != sg_heartbeat_flag))  // The initial value of sg_heartbeat_flag is 255, so it is not executed for the first time, and the power-up check is executed first
    {
        sg_heartbeat_flag = 1;
        this->heartbeat_state_text_sensor_->publish_state(s_heartbeat_str[sg_heartbeat_flag]);
    }
    if (s_power_on_status < 4)  // Post power-up status check
    {
        if (s_output_info_switch_flag == OUTPUT_SWITCH_INIT)  // Power-up status check first item
        {
            sg_start_query_data = CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH;  // Custom function to query radar output information switch
            sg_start_query_data_max = CUSTOM_FUNCTION_MAX;
        }
        else if (s_output_info_switch_flag == OUTPUT_SWTICH_OFF)  // When the bottom open parameter button is closed, the power-up status checks the second item
        {
            sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
            sg_start_query_data_max = STANDARD_FUNCTION_MAX;
        }
        else if (s_output_info_switch_flag == OUTPUT_SWTICH_ON)   // When the bottom open parameter button is on, the power-up state checks the second item
        {
            sg_start_query_data = CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH;
            sg_start_query_data_max = CUSTOM_FUNCTION_MAX;
        }
        s_power_on_status++;  // There are a total of four inspections
    }
    else
    {
        sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
        sg_start_query_data_max = STANDARD_FUNCTION_QUERY_KEEPAWAY_STATUS;
    }
}

// main loop
void mr24hpc1Component::loop() {
    uint8_t byte;

    // Is there data on the serial port
    while (this->available())
    {
        this->read_byte(&byte);
        this->R24_split_data_frame(byte);  // split data frame
    }

    // !s_output_info_switch_flag = !OUTPUT_SWITCH_INIT = !0 = 1  (Power-up check first item - check if the underlying open parameters are turned on)
    if (!s_output_info_switch_flag && sg_start_query_data == CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH)
    {
        // Check if the button for the underlying open parameter is on, if so
        this->get_radar_output_information_switch();  // This function in conjunction with R24_split_data_frame changes the state of the s_output_info_switch_flag, ON or OFF.
        sg_start_query_data++;    // now: sg_start_query_data = CUSTOM_FUNCTION_QUERY_PRESENCE_OF_DETECTION_RANGE  sg_start_query_data_max = CUSTOM_FUNCTION_MAX
    }
    // When the switch for the underlying open parameter is off, the value of sg_start_query_data should be within limits
    if ((s_output_info_switch_flag == OUTPUT_SWTICH_OFF) && (sg_start_query_data <= sg_start_query_data_max) && (sg_start_query_data >= STANDARD_FUNCTION_QUERY_PRODUCT_MODE))
    {
        switch (sg_start_query_data)
        {
            case STANDARD_FUNCTION_QUERY_PRODUCT_MODE:
                if (strlen(this->c_product_mode) > 0)
                {
                    this->product_model_text_sensor_->publish_state(this->c_product_mode);  // Release Product Model
                }
                else
                {
                    this->get_product_mode();  // Check Product Model
                }
                break;
            case STANDARD_FUNCTION_QUERY_PRODUCT_ID:
                if (strlen(this->c_product_id) > 0)
                {
                    this->product_id_text_sensor_->publish_state(this->c_product_id);  // Publish Product ID
                }
                else
                {
                    this->get_product_id();  // Check Product ID
                }
                break;
            case STANDARD_FUNCTION_QUERY_FIRMWARE_VERDION:
                if (strlen(this->c_firmware_version) > 0)
                {
                    this->firware_version_text_sensor_->publish_state(this->c_firmware_version);  // Release Firmware Version Number
                }
                else
                {
                    this->get_firmware_version();  // check firmware version number
                }
                break;
            case STANDARD_FUNCTION_QUERY_HARDWARE_MODE:
                if (strlen(this->c_hardware_model) > 0)
                {
                    this->hardware_model_text_sensor_->publish_state(this->c_hardware_model);  // Release Hardware Models
                }
                else
                {
                    this->get_hardware_model();  // check Hardware Model
                }
                break;
            case STANDARD_FUNCTION_QUERY_HUMAN_STATUS:
                this->get_human_status();
                break;
            case STANDARD_FUNCTION_QUERY_KEEPAWAY_STATUS:
                this->get_keep_away();
                break;
            case STANDARD_FUNCTION_MAX:
                this->get_heartbeat_packet();
                break;
        }
        sg_start_query_data++;
    }
    if (sg_start_query_data > CUSTOM_FUNCTION_MAX) sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
}

// split data frame
void mr24hpc1Component::R24_split_data_frame(uint8_t value)
{
    switch (sg_recv_data_state)
    {
        case FRAME_IDLE:                    // starting value
            if (FRAME_HEADER1_VALUE == value)
            {
                sg_recv_data_state = FRAME_HEADER2;
            }
            break;
        case FRAME_HEADER2:
            if (FRAME_HEADER2_VALUE == value)
            {
                sg_frame_buf[0] = FRAME_HEADER1_VALUE;
                sg_frame_buf[1] = FRAME_HEADER2_VALUE;
                sg_recv_data_state = FRAME_CTL_WORLD;
            }
            else
            {
                sg_recv_data_state = FRAME_IDLE;
                ESP_LOGD(TAG, "FRAME_IDLE ERROR value:%x", value);
            }
            break;
        case FRAME_CTL_WORLD:
            sg_frame_buf[2] = value;
            sg_recv_data_state = FRAME_CMD_WORLD;
            break;
        case FRAME_CMD_WORLD:
            sg_frame_buf[3] = value;
            sg_recv_data_state = FRAME_DATA_LEN_H;
            break;
        case FRAME_DATA_LEN_H:
            if (value <= 4)
            {
                sg_data_len = value * 256;
                sg_frame_buf[4] = value;
                sg_recv_data_state = FRAME_DATA_LEN_L;
            }
            else
            {
                sg_data_len = 0;
                sg_recv_data_state = FRAME_IDLE;
                ESP_LOGD(TAG, "FRAME_DATA_LEN_H ERROR value:%x", value);
            }
            break;
        case FRAME_DATA_LEN_L:
            sg_data_len += value;
            if (sg_data_len > 32)
            {
                ESP_LOGD(TAG, "len=%d, FRAME_DATA_LEN_L ERROR value:%x", sg_data_len, value);
                sg_data_len = 0;
                sg_recv_data_state = FRAME_IDLE;
            }
            else
            {
                sg_frame_buf[5] = value;
                sg_frame_len = 6;
                sg_recv_data_state = FRAME_DATA_BYTES;
            }
            break;
        case FRAME_DATA_BYTES:
            sg_data_len -= 1;
            sg_frame_buf[sg_frame_len++] = value;
            if (sg_data_len <= 0)
            {
                sg_recv_data_state = FRAME_DATA_CRC;
            }
            break;
        case FRAME_DATA_CRC:
            sg_frame_buf[sg_frame_len++] = value;
            sg_recv_data_state = FRAME_TAIL1;
            break;
        case FRAME_TAIL1:
            if (FRAME_TAIL1_VALUE == value)
            {
                sg_recv_data_state = FRAME_TAIL2;
            }
            else
            {
                sg_recv_data_state = FRAME_IDLE;
                sg_frame_len = 0;
                sg_data_len = 0;
                ESP_LOGD(TAG, "FRAME_TAIL1 ERROR value:%x", value);
            }
            break;
        case FRAME_TAIL2:
            if (FRAME_TAIL2_VALUE == value)
            {
                sg_frame_buf[sg_frame_len++] = FRAME_TAIL1_VALUE;
                sg_frame_buf[sg_frame_len++] = FRAME_TAIL2_VALUE;
                memcpy(sg_frame_prase_buf, sg_frame_buf, sg_frame_len);
                if (get_frame_check_status(sg_frame_prase_buf, sg_frame_len))
                {
                    this->R24_parse_data_frame(sg_frame_prase_buf, sg_frame_len);
                }
                else
                {
                    ESP_LOGD(TAG, "frame check failer!");
                }
            }
            else
            {
                ESP_LOGD(TAG, "FRAME_TAIL2 ERROR value:%x", value);
            }
            memset(sg_frame_prase_buf, 0, FRAME_BUF_MAX_SIZE);
            memset(sg_frame_buf, 0, FRAME_BUF_MAX_SIZE);
            sg_frame_len = 0;
            sg_data_len = 0;
            sg_recv_data_state = FRAME_IDLE;
            break;
        default:
            sg_recv_data_state = FRAME_IDLE;
    }
}

// Parses data frames related to product information
void mr24hpc1Component::R24_frame_parse_product_Information(uint8_t *data)
{
    uint8_t product_len = 0;
    if (data[FRAME_COMMAND_WORD_INDEX] == 0xA1)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_product_mode, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_product_mode, &data[FRAME_DATA_INDEX], product_len);
            this->product_model_text_sensor_->publish_state(this->c_product_mode);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get product_mode length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA2)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_product_id, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_product_id, &data[FRAME_DATA_INDEX], product_len);
            this->product_id_text_sensor_->publish_state(this->c_product_id);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get productId length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA3)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_hardware_model, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_hardware_model, &data[FRAME_DATA_INDEX], product_len);
            this->hardware_model_text_sensor_->publish_state(this->c_hardware_model);
            ESP_LOGD(TAG, "Reply: get hardware_model :%s", this->c_hardware_model);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get hardwareModel length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA4)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {

            memset(this->c_firmware_version, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_firmware_version, &data[FRAME_DATA_INDEX], product_len);
            this->firware_version_text_sensor_->publish_state(this->c_firmware_version);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get firmwareVersion length too long!");
        }
    }
}

// Parsing the underlying open parameters
void mr24hpc1Component::R24_frame_parse_open_underlying_information(uint8_t *data)
{
    if (data[FRAME_COMMAND_WORD_INDEX] == 0x00)
    {
        this->underly_open_function_switch_->publish_state(data[FRAME_DATA_INDEX]);  // Underlying Open Parameter Switch Status Updates
        if (data[FRAME_DATA_INDEX])
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_ON;
        }
        else
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
    {
        this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
        this->custom_presence_of_detection_sensor_->publish_state(data[FRAME_DATA_INDEX + 1] * 0.5f);
        this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX + 2]);
        this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX + 3] * 0.5f);
        this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX + 4] - 10) * 0.5f);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x06)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07)
    {
        this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x08)
    {
        // id(custom_judgment_threshold_exists).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09)
    {
        // id(custom_motion_amplitude_trigger_threshold).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0a)
    {
        // if (id(custom_presence_of_perception_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_presence_of_perception_boundary).publish_state(s_presence_of_perception_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0b)
    {
        // if (id(custom_motion_trigger_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_motion_trigger_boundary).publish_state(s_motion_trig_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0c)
    {
        // uint32_t motion_trigger_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_motion_trigger_time_bak != motion_trigger_time)
        // {
        //     sg_motion_trigger_time_bak = motion_trigger_time;
        //     id(custom_motion_trigger_time).publish_state(motion_trigger_time);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0d)
    {
        // uint32_t move_to_rest_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_move_to_rest_time_bak != move_to_rest_time)
        // {
        //     id(custom_movement_to_rest_time).publish_state(move_to_rest_time);
        //     sg_move_to_rest_time_bak = move_to_rest_time;
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0e)
    {
        // uint32_t enter_unmanned_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_enter_unmanned_time_bak != enter_unmanned_time)
        // {
        //     id(custom_time_of_enter_unmanned).publish_state(enter_unmanned_time);
        //     sg_enter_unmanned_time_bak = enter_unmanned_time;
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x80)
    {
        if (data[FRAME_DATA_INDEX])
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_ON;
        }
        else
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
        }
        this->underly_open_function_switch_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
        this->custom_spatial_static_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82) {
        this->custom_spatial_motion_value_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83)
    {
        this->custom_presence_of_detection_sensor_->publish_state(s_presence_of_detection_range_str[data[FRAME_DATA_INDEX]]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x84) { 
        this->custom_motion_distance_sensor_->publish_state(data[FRAME_DATA_INDEX] * 0.5f);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x85) {  
        this->custom_motion_speed_sensor_->publish_state((data[FRAME_DATA_INDEX] - 10) * 0.5f);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x86)
    {
        this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87)
    {
        this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x88)
    {
        // id(custom_judgment_threshold_exists).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89)
    {
        // id(custom_motion_amplitude_trigger_threshold).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8a)
    {
        // if (id(custom_presence_of_perception_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_presence_of_perception_boundary).publish_state(s_presence_of_perception_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8b)
    {
        // if (id(custom_motion_trigger_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_motion_trigger_boundary).publish_state(s_motion_trig_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8c)
    {
        // uint32_t motion_trigger_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_motion_trigger_time_bak != motion_trigger_time)
        // {
        //     id(custom_motion_trigger_time).publish_state(motion_trigger_time);
        //     sg_motion_trigger_time_bak = motion_trigger_time;
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8d)
    {
        // uint32_t move_to_rest_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_move_to_rest_time_bak != move_to_rest_time)
        // {
        //     id(custom_movement_to_rest_time).publish_state(move_to_rest_time);
        //     sg_move_to_rest_time_bak = move_to_rest_time;
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8e)
    {
        // uint32_t enter_unmanned_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_enter_unmanned_time_bak != enter_unmanned_time)
        // {
        //     id(custom_time_of_enter_unmanned).publish_state(enter_unmanned_time);
        //     sg_enter_unmanned_time_bak = enter_unmanned_time;
        // }
    }
}


void mr24hpc1Component::R24_parse_data_frame(uint8_t *data, uint8_t len)
{
    switch (data[FRAME_CONTROL_WORD_INDEX])
    {
        case 0x01:
        {
            if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
            {
                sg_heartbeat_flag = 0;
            }
            else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02)
            {
                ESP_LOGD(TAG, "Reply: query reset packet");
            }
        }
        break;
        case 0x02:
        {
            this->R24_frame_parse_product_Information(data);
        }
        break;
        case 0x05:
        {
            this->R24_frame_parse_work_status(data);
        }
        break;
        case 0x08:
        {
            this->R24_frame_parse_open_underlying_information(data);
        }
        break;
        case 0x80:
        {
            this->R24_frame_parse_human_information(data);
        }
        break;
        default:
            ESP_LOGD(TAG, "control world:0x%02X not found", data[FRAME_CONTROL_WORD_INDEX]);
        break;
    }
}

void mr24hpc1Component::R24_frame_parse_work_status(uint8_t *data)
{
    if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
    {
        ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07)
    {
        // if (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     this->scene_mode_select_->publish_state(s_scene_str[data[FRAME_DATA_INDEX] - 1]);
        // }
        // else
        // {
        //     ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x08)
    {
        // 1-3
        // id(sensitivity).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09)
    {
        // 1-4
        // id(custom_mode_settings).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81)
    {
        ESP_LOGD(TAG, "Reply: get radar init status 0x%02X", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87)
    {
        if (this->scene_mode_select_->has_index(data[FRAME_DATA_INDEX] - 1))
        {
            this->scene_mode_select_->publish_state(s_scene_str[data[FRAME_DATA_INDEX] - 1]);
        }
        else
        {
            ESP_LOGD(TAG, "Select has index offset %d Error", data[FRAME_DATA_INDEX]);
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x88)
    {
        // id(sensitivity).publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89)
    {
        // 1-4
        // id(custom_mode_settings).publish_state(data[FRAME_DATA_INDEX]);
    }
    else
    {
        ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
    }
}

void mr24hpc1Component::R24_frame_parse_human_information(uint8_t *data)
{
    if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
    {
        this->someoneExists_binary_sensor_->publish_state(s_someoneExists_str[data[FRAME_DATA_INDEX]]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02)
    {
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->motion_status_text_sensor_->publish_state(s_motion_status_str[data[FRAME_DATA_INDEX]]);
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x03)
    {
        this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0A)
    {
        // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
        // if (data[FRAME_DATA_INDEX] < 9 && data[FRAME_DATA_INDEX] >= 0)
        // {
        //     id(unmanned_time).publish_state(s_unmanned_time_str[data[FRAME_DATA_INDEX]]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0B)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81)
    {
        this->someoneExists_binary_sensor_->publish_state(s_someoneExists_str[data[FRAME_DATA_INDEX]]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82)
    {
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->motion_status_text_sensor_->publish_state(s_motion_status_str[data[FRAME_DATA_INDEX]]);
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83)
    {
        this->movementSigns_sensor_->publish_state(data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8A)
    {
        // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
        // if (data[FRAME_DATA_INDEX] < 9 && data[FRAME_DATA_INDEX] >= 0)
        // {
        //     id(unmanned_time).publish_state(s_unmanned_time_str[data[FRAME_DATA_INDEX]]);
        // }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8B)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
    }
    else
    {
        ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
    }
}

// Sending data frames
void mr24hpc1Component::send_query(uint8_t *query, size_t string_length)
{
    int i;
    for (i = 0; i < string_length; i++)
    {
        write(query[i]);
    }
    show_frame_data(query, i);
}

// Send Heartbeat Packet Command
void mr24hpc1Component::get_heartbeat_packet(void)
{
    uint8_t send_data_len = 10;
    uint8_t send_data[10] = {0x53, 0x59, 0x01, 0x01, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// Issuance of the underlying open parameter query command
void mr24hpc1Component::get_radar_output_information_switch(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x80, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// Issuance of product model orders
void mr24hpc1Component::get_product_mode(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA1, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// Issuing the Get Product ID command
void mr24hpc1Component::get_product_id(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA2, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// Issuing hardware model commands
void mr24hpc1Component::get_hardware_model(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA3, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// Issuing software version commands
void mr24hpc1Component::get_firmware_version(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA4, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

void mr24hpc1Component::get_human_status(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x81, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

void mr24hpc1Component::get_keep_away(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x80, 0x8B, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

void mr24hpc1Component::set_underlying_open_function(bool enable)
{
    uint8_t underlyswitch_on[] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x01, 0xB6, 0x54, 0x43};
    uint8_t underlyswitch_off[] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x00, 0xB5, 0x54, 0x43};
    if(enable) send_query(underlyswitch_on, sizeof(underlyswitch_on));
    else send_query(underlyswitch_off, sizeof(underlyswitch_off));
    this->keep_away_text_sensor_->publish_state("");
    this->motion_status_text_sensor_->publish_state("");
    this->custom_spatial_static_value_sensor_->publish_state(0.0f);
    this->custom_spatial_motion_value_sensor_->publish_state(0.0f);
    this->custom_motion_distance_sensor_->publish_state(0.0f);
    this->custom_presence_of_detection_sensor_->publish_state(0.0f);
    this->custom_motion_speed_sensor_->publish_state(0.0f);
}

void mr24hpc1Component::set_scene_mode(const std::string &state){
    uint8_t cmd_value = SCENEMODE_ENUM_TO_INT.at(state);
    if(cmd_value == 0x00)return;
    uint8_t scenemodeArr[10] = {0x53, 0x59, 0x05, 0x07, 0x00, 0x01, cmd_value, 0x00, 0x54, 0x43};
    scenemodeArr[7] = get_frame_crc_sum(scenemodeArr, sizeof(scenemodeArr));
    this->send_query(scenemodeArr, sizeof(scenemodeArr));
}

}  // namespace empty_text_sensor
}  // namespace esphome