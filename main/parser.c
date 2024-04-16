#define LOG_LOCAL_LEVEL ESP_LOG_ERROR //as per ESP32 logging guidelines LOG_LOCAL_LEVEL is defined before including esp_log.h
#include "parser.h"
static const char *TAG = "Parser";

static int calculate_checksum(const char *sentence) {
    int checksum = 0;
    for (int i = 1; sentence[i] != '*'; i++) {
        checksum ^= sentence[i];
    }
    return checksum;
}

static inline void parse_time(char *utc_time, uint8_t *hours, uint8_t *minutes, float *seconds) {
    // Check if the time string is in the correct format
    if (strlen(utc_time) != 9) {
        ESP_LOGE(TAG,"Time string length invalid.");
        return;
    }

    char *tmp_hour_str = strndup(utc_time, 2);
    char *tmp_minutes_str = strndup(utc_time + 2, 2);

    // Extract hours, minutes, and seconds
    int parsed_hours = atoi(tmp_hour_str) + GMT;
    if (parsed_hours >= 24) {
        parsed_hours %= 24; // Keep hours within 0-23 range
    }
    *hours = parsed_hours;
    *minutes = atoi(tmp_minutes_str);
    *seconds = atof(utc_time + 4);

    free(tmp_hour_str);
    free(tmp_minutes_str);    
}

static inline void parse_date(char *date_string, uint8_t *day, uint8_t *month, uint16_t *year)
{
    // Check if the date string is in the correct format
    if (strlen(date_string) != 6) {
        ESP_LOGE(TAG,"Date string length invalid.");
        return;
    }

    char *tmp_day_str = strndup(date_string, 2);
    char *tmp_month_str = strndup(date_string + 2, 2);
    char *tmp_year_str = strndup(date_string+4, 2);

    // Extract day, month, and year
    *day = atoi(tmp_day_str);
    *month = atoi(tmp_month_str);
    *year = atoi(tmp_year_str) + 2000; // Assuming year is represented in YY format

    free(tmp_day_str);
    free(tmp_month_str);
    free(tmp_year_str);
}

// Function to parse latitude or longitude from the NMEA sentence
static float parse_coordinate(const char *coord) {
    char *tmp_coord_str = strndup(coord, 2);
    float degrees = atoi(tmp_coord_str);
    float minutes = atof(coord + 2); //coord is already null terminated in the function parse_gga
    
    free(tmp_coord_str);

    return degrees + minutes / 60.0;
}

static void parse_gga(const char *sentence, uint16_t len, gps_t *data_struct) {
    float tmp_latitude = 0.0;
    float tmp_longitude = 0.0;
    float tmp_altitude = 0.0;

    int i = 0; int item_idx = 0;
    char current_idx[20] = "";
    int item_length = 0;

    // Iterate through the sentence character by character upto null character
    while(sentence[i] != '\0') {
        if (sentence[i] == ',' || sentence[i] == '*') {
            // Null-terminate the current field buffer
            current_idx[item_length] = '\0';
            switch (item_idx) {
                case 1: // Time
                    if (item_length > 0) {
                        parse_time(current_idx, &(data_struct->tim.hour), &(data_struct->tim.minute), &(data_struct->tim.second));
                        ESP_LOGI(TAG,"Time:%d:%d:%f", data_struct->tim.hour, data_struct->tim.minute, data_struct->tim.second);
                    }
                    break;
                case 2: // Latitude
                    if (item_length > 0) {
                        tmp_latitude = parse_coordinate(current_idx);
                    }
                    break;
                case 3: // Latitude direction (N/S)
                    if (item_length > 0) {
                        if (current_idx[0] == 'S' || current_idx[0] == 's') {
                            data_struct->latitude = tmp_latitude * -1;
                        } else if (current_idx[0] == 'N' || current_idx[0] == 'n') {
                            data_struct->latitude = tmp_latitude;
                        }
                        ESP_LOGI(TAG, "latitude: %f",data_struct->latitude);
                    }
                    break;
                case 4: // Longitude
                    if (item_length > 0) {
                        tmp_longitude = parse_coordinate(current_idx);
                    }
                    break;
                case 5: // Longitude direction (E/W)
                    if (item_length > 0) {
                        if (current_idx[0] == 'W' || current_idx[0] == 'w') {
                            data_struct->longitude = tmp_longitude * -1;
                        } else if (current_idx[0] == 'E' || current_idx[0] == 'e') {
                            data_struct->longitude = tmp_longitude;
                        }
                        ESP_LOGI(TAG, "longitude: %f",data_struct->longitude);
                    }
                    break;
                case 6: // Fix quality
                    if (item_length > 0) {
                        data_struct->fix = atoi(current_idx);
                        ESP_LOGI(TAG, "fix: %d",data_struct->fix);
                    }
                    break;
                case 7: // Number of satellites
                    if (item_length > 0) {
                        data_struct->sats_in_use = atoi(current_idx);
                        ESP_LOGI(TAG, "Sats: %d",data_struct->sats_in_use);
                    }
                    break;
                case 8: // HDOP
                    if (item_length > 0) {
                        data_struct->dop_h = atof(current_idx);
                        ESP_LOGI(TAG, "HDOP %f",data_struct->dop_h);
                    }
                    break;
                case 9: // Altitude
                    if (item_length > 0) {
                        tmp_altitude = atof(current_idx);
                    }
                    break;
                case 11: // Altitude above ellipsoid
                    if (item_length > 0) {
                        data_struct->altitude = tmp_altitude + atof(current_idx);
                        ESP_LOGI(TAG, "altitude: %f",data_struct->altitude);                        
                    }
                    // ESP_LOGI(TAG,"altitude above ellipsoid: %f\r\n", gps.altitude);
                    break;
                default:
                    break;
            }

            // Reset the current field buffer and length
            memset(current_idx, 0, sizeof(current_idx));
            item_length = 0;

            // Move to the next field index
            item_idx++;

            // Skip the '*' character
            if (sentence[i] == '*') {
                break;
            }
        } else {
            // Append the character to the current field buffer
            current_idx[item_length++] = sentence[i];
        }
        i++;
    }
}

// Function to parse the GSA sentence
static void parse_gsa(const char *sentence, uint16_t length, gps_t *data_struct) {
    int i = 0, item_idx = 0;
    bool empty_field = false; // Flag to track if the current field is empty
    char current_idx[20] = ""; // Buffer to store the current field
    int item_length = 0; // Length of the current field

    while (sentence[i] != '\0') {
        if (sentence[i] == ',' || sentence[i] == '*') {
            // Handle empty field
            if (empty_field) {

            } else {
                // Null-terminate the current field buffer
                current_idx[item_length] = '\0';
                // Store the current field
                switch (item_idx) {
                    case 1: // Mode (A/M)
                        if (item_length > 0) {
                            data_struct->mode = current_idx[0];
                            ESP_LOGI(TAG,"GPS Mode: %c", data_struct->mode);
                        } else {
                            ESP_LOGE(TAG,"GPS Mode field empty.\r\n");
                        }
                        break;
                    case 2: // Fix Mode
                        if (!empty_field) {
                            data_struct->fix_mode = atoi(current_idx);
                            ESP_LOGI(TAG,"fix mode: %d", data_struct->fix_mode);
                        } else {
                            ESP_LOGE(TAG,"Fix Mode field is empty.");
                        }
                        break;
                    case 15: // PDOP
                        if (!empty_field) {
                            data_struct->dop_p = atof(current_idx);
                            ESP_LOGI(TAG,"PDOP: %f", data_struct->dop_p);
                        } else {
                            ESP_LOGE(TAG,"PDOP field is empty.");
                        }
                        break;
                    case 16: // HDOP
                        if (!empty_field) {
                            data_struct->dop_h = atof(current_idx);
                            ESP_LOGI(TAG,"HDOP: %f", data_struct->dop_h);
                        } else {
                            ESP_LOGE(TAG,"HDOP field is empty.");
                        }
                        break;
                    case 17: // VDOP
                        if (!empty_field) {
                            data_struct->dop_v = atof(current_idx);
                            ESP_LOGI(TAG,"VDOP: %f", data_struct->dop_v);
                        } else {
                            ESP_LOGE(TAG,"VDOP field is empty.");
                        }
                        break;
                    default:
                        break;
                }
                // Reset the current field buffer and length
                memset(current_idx, 0, sizeof(current_idx));
                item_length = 0;
                // Move to the next field
                item_idx++;
            }
            // Reset the empty field flag
            empty_field = false;
        } else {
            // Append the character to the current field buffer
            current_idx[item_length++] = sentence[i];
        }
        i++;
    }
}

static void parse_rmc(const char *sentence, uint16_t len, gps_t *data_struct) {
    int i = 0, item_idx = 0;
    bool empty_field = false; // Flag to track if the current field is empty
    char current_idx[20] = ""; // Buffer to store the current field
    int item_length = 0; // Length of the current field

    // Temporary variables to store latitude and longitude
    float tmp_latitude = 0.0;
    float tmp_longitude = 0.0;

    // Iterate through the sentence character by character
    while (sentence[i] != '\0') {
        if (sentence[i] == ',' || sentence[i] == '*') {
            // Handle empty field
            if (empty_field) {
  
            } else {
                // Null-terminate the current field buffer
                current_idx[item_length] = '\0';
                // Store or process the current field based on its index
                switch (item_idx) {
                    case 1: // Time
                        if (item_length >= 6) {
                            // Assuming time format is HHMMSS
                            parse_time(current_idx, &(data_struct->tim.hour), &(data_struct->tim.minute), &(data_struct->tim.second));
                            ESP_LOGI(TAG,"Time: %d:%d:%f", data_struct->tim.hour, data_struct->tim.minute, data_struct->tim.second);
                        }
                        break;
                    case 2: // Validity
                        if(item_length > 0){
                            data_struct->valid = (current_idx[0] == 'A') ? true : false;
                            ESP_LOGI(TAG,"Validity status: %d", data_struct->valid);
                        }
                        break;
                    case 3: // Latitude
                        if (item_length > 0) {
                            tmp_latitude = parse_coordinate(current_idx);
                        }
                        break;
                    case 4: // Latitude direction (N/S)
                        if (item_length > 0) {
                            if (current_idx[0] == 'S' || current_idx[0] == 's') {
                                data_struct->latitude = tmp_latitude * -1;
                            } else if (current_idx[0] == 'N' || current_idx[0] == 'n') {
                                data_struct->latitude = tmp_latitude;
                            }
                            ESP_LOGI(TAG, "latitude: %f",data_struct->latitude);
                        }
                        break;
                    case 5: // Longitude
                        if (item_length > 0) {
                            tmp_longitude = parse_coordinate(current_idx);
                        }
                        break;
                    case 6: // Longitude direction (E/W)
                        if (item_length > 0) {
                            if (current_idx[0] == 'W' || current_idx[0] == 'w') {
                                data_struct->longitude = tmp_longitude * -1;
                            } else if (current_idx[0] == 'E' || current_idx[0] == 'e') {
                                data_struct->longitude = tmp_longitude;
                            }
                            ESP_LOGI(TAG, "longitude: %f",data_struct->longitude);
                        }
                        break;
                    case 7: // Speed (Knots)
                        if (item_length > 0) {
                            data_struct->speed = atof(current_idx);
                            ESP_LOGI(TAG, "speed: %f",data_struct->speed);
                        }
                        break;
                    case 8: // Course over ground
                        if (item_length > 0) {
                            data_struct->cog = atof(current_idx);
                            ESP_LOGI(TAG, "cog: %f",data_struct->cog);
                        }
                        break;
                    case 9: // Date
                        if (item_length >= 6) {
                            // Assuming date format is DDMMYY
                            parse_date(current_idx, &(data_struct->date.day), &(data_struct->date.month), &(data_struct->date.year));
                            ESP_LOGI(TAG, "date: %d/%d/%d", data_struct->date.day, data_struct->date.month, data_struct->date.year);
                        }
                        break;
                    default:
                        break;
                }
                // Reset the current field buffer and length
                memset(current_idx, 0, sizeof(current_idx));
                item_length = 0;
                // Move to the next field index
                item_idx++;
            }
            // Reset the empty field flag
            empty_field = false;
        } else {
            // Append the character to the current field buffer
            current_idx[item_length++] = sentence[i];
        }
        i++;
    }
}


static void parse_vtg(const char *sentence, uint16_t len, gps_t *data_struct) {
    int i = 0, field_index = 0;
    bool empty_field = false; // Flag to track if the current field is empty
    char current_field[20] = ""; // Buffer to store the current field
    int field_length = 0; // Length of the current field

    // Iterate through the sentence character by character
    while (sentence[i] != '\0') {
        if (sentence[i] == ',' || sentence[i] == '*') {
            // Handle empty field
            if (empty_field) {
            } else {
                // Null-terminate the current field buffer
                current_field[field_length] = '\0';
                // Store or process the current field based on its index
                switch (field_index) {
                    case 5: // Ground speed in knots
                        if (field_length > 0) {
                            // Parse and store the value if it's not empty
                            data_struct->speed = atof(current_field);
                            ESP_LOGI(TAG, "speed: %f", data_struct->speed);
                        }
                        break;
                    case 7: // Ground speed in kilometers per hour
                        if (field_length > 0) {
                            // Parse and store the value if it's not empty
                            // Assuming the conversion factor is 1.852
                            data_struct->speedkmh = atof(current_field) * 1.852;
                            ESP_LOGI(TAG,"Speed km/h: %f",data_struct->speedkmh);
                        }
                        break;
                    default:
                        break;
                }
                // Reset the current field buffer and length
                memset(current_field, 0, sizeof(current_field));
                field_length = 0;
                // Move to the next field index
                field_index++;
            }
            // Reset the empty field flag
            empty_field = false;
        } else {
            // Append the character to the current field buffer
            current_field[field_length++] = sentence[i];
        }
        i++;
    }
}


gps_t gps_parse(const char *sentence)
{
    int i = 1;
    char type[7]; // size greater by 1 to accommodate null terminator
    static gps_t gps_data;
    static bool gps_struct_initialized = false;

    if (gps_struct_initialized == false) {
        memset(&gps_data, 0, sizeof(gps_t));
        gps_struct_initialized = true;
    }

    //Checks to avoid segmentation fault 
    if (sentence == NULL) { 
        // ESP_LOGI(TAG, "Invalid GPS sentence.\r\n");
        ESP_LOGE(TAG,"Pointer to NULL passed to function.\r\n");
        gps_data.status = (gps_status_t)GPS_PTR_TO_NULL;
        return gps_data;
    }

    // Calculate the length of the sentence and create a copy of the sentence which is null terminated
    uint8_t len = 0;
    while (sentence[len] != '\n' && len < MAX_SENTENCE_LENGTH) {
        len++;
    }
    len++;

    char *sentence_copy = strndup(sentence, len);
    if (sentence_copy == NULL) {
        ESP_LOGE(TAG,"Sentence copy not created.\r\n");
        gps_data.status = (gps_status_t)GPS_MEM_LOW;
        return gps_data;
    }

    //Checks to avoid using invalid sentence 
    if (sentence[0] != '$' || len < 3 || len > 83) { 
        ESP_LOGE(TAG, "Invalid GPS sentence.\r\n");
        gps_data.status = (gps_status_t)GPS_INV_SENTENCE;
        return gps_data;
    }

    int d=0;
    //parse upto asterick
    while(sentence_copy[d] != '*' && d < len-2){
        d++;
    }
    char checksum_str[2] = {sentence_copy[d+1],sentence_copy[d+2]};
    int provided_checksum = strtol(checksum_str, NULL, 16);

    // Perform checksum calculation
    int calculated_checksum = calculate_checksum(sentence);
    if(provided_checksum != calculated_checksum){
        ESP_LOGE(TAG,"CRC error");
        gps_data.status = (gps_status_t)GPS_CRC_ERROR;
        return gps_data;
    }

    while (sentence_copy[i] != ',') {
        type[i - 1] = sentence_copy[i]; // Copy character from sentence to type
        i++;
    }
    type[i - 1] = '\0'; // Null-terminate the string in type
    
    if (strcmp(type, "GPGGA") == 0) {
        ESP_LOGI(TAG, "%s",sentence_copy);
        parse_gga(sentence_copy, len, &gps_data);
    } else if (strcmp(type, "GPGSA") == 0) {
        ESP_LOGI(TAG, "%s",sentence_copy);
        parse_gsa(sentence_copy, len, &gps_data);
    } else if (strcmp(type, "GPGSV") == 0) {
        // Not implemented yet
        // parse_gsv();
        // ESP_LOGI(TAG,"GSV");
    } else if (strcmp(type, "GPRMC") == 0) {
        ESP_LOGI(TAG, "%s",sentence_copy);
        parse_rmc(sentence_copy, len, &gps_data);
    } else if (strcmp(type, "GPGLL") == 0) {
        // Not implemented yet 
        // parse_gll();
        // ESP_LOGI(TAG,"GLL");
    } else if (strcmp(type, "GPVTG") == 0) {
        ESP_LOGI(TAG, "%s",sentence_copy);
        parse_vtg(sentence_copy, len, &gps_data);
    } else {
        gps_data.status = (gps_status_t)GPS_SENTENCE_MISMATCH;
        return gps_data;
    }

    free(sentence_copy);
    gps_data.status = GPS_OKAY;
    return gps_data;
}

