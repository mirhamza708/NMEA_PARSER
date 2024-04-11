#include "parser.h"
static const char *TAG = "Parser";

static gps_t gps;

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
        return;
    }

    // Extract hours, minutes, and seconds
    int parsed_hours = atoi(strndup(utc_time, 2)) + GMT5;
    if (parsed_hours >= 24) {
        parsed_hours %= 24; // Keep hours within 0-23 range
    }
    *hours = parsed_hours;
    *minutes = atoi(strndup(utc_time + 2, 2));
    *seconds = atof(utc_time + 4);
}

static inline void parse_date(char *date_string, uint8_t *day, uint8_t *month, uint16_t *year)
{
    // Check if the date string is in the correct format
    if (strlen(date_string) != 6) {
        return;
    }
    // Extract day, month, and year
    *day = atoi(strndup(date_string, 2));
    *month = atoi(strndup(date_string + 2, 2));
    *year = atoi(strndup(date_string + 4, 2)) + 2000; // Assuming year is represented in YY format
}

// Function to parse latitude or longitude from the NMEA sentence
static float parse_coordinate(const char *coord) {
    float degrees = atof(strndup(coord, 2));
    float minutes = atof(strndup(coord + 2, strlen(coord) - 2));
    return degrees + minutes / 60.0;
}

static void parse_gga(const char *sentence, uint16_t len) {
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
                        parse_time(current_idx, &(gps.tim.hour), &(gps.tim.minute), &(gps.tim.second));
                            #ifdef DEBUG_GGA
                                ESP_LOGI(TAG,"Time:%d:%d:%f", gps.tim.hour, gps.tim.minute, gps.tim.second);
                            #endif
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
                            gps.latitude = tmp_latitude * -1;
                        } else if (current_idx[0] == 'N' || current_idx[0] == 'n') {
                            gps.latitude = tmp_latitude;
                        }
                            #ifdef DEBUG_GGA
                                ESP_LOGI(TAG, "latitude: %f",gps.latitude);
                            #endif
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
                            gps.longitude = tmp_longitude * -1;
                        } else if (current_idx[0] == 'E' || current_idx[0] == 'e') {
                            gps.longitude = tmp_longitude;
                        }
                            #ifdef DEBUG_GGA
                                ESP_LOGI(TAG, "longitude: %f",gps.longitude);
                            #endif
                    }
                    break;
                case 6: // Fix quality
                    if (item_length > 0) {
                        gps.fix = atoi(current_idx);
                            #ifdef DEBUG_GGA
                                ESP_LOGI(TAG, "fix: %d",gps.fix);
                            #endif
                    }
                    break;
                case 7: // Number of satellites
                    if (item_length > 0) {
                        gps.sats_in_use = atoi(current_idx);
                            #ifdef DEBUG_GGA
                                ESP_LOGI(TAG, "Sats: %d",gps.sats_in_use);
                            #endif
                    }
                    break;
                case 8: // HDOP
                    if (item_length > 0) {
                        gps.dop_h = atof(current_idx);
                        #ifdef DEBUG_GGA
                            ESP_LOGI(TAG, "HDOP %f",gps.dop_h);
                        #endif
                    }
                    break;
                case 9: // Altitude
                    if (item_length > 0) {
                        tmp_altitude = atof(current_idx);
                    }
                    break;
                case 11: // Altitude above ellipsoid
                    if (item_length > 0) {
                        gps.altitude = tmp_altitude + atof(current_idx);
                        #ifdef DEBUG_GGA
                            ESP_LOGI(TAG, "altitude: %f",gps.altitude);                        
                        #endif
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
static void parse_gsa(const char *sentence, uint16_t length) {
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
                            gps.mode = current_idx[0];
                            #ifdef DEBUG_GSA
                                ESP_LOGI(TAG,"GPS Mode: %c", gps.mode);
                            #endif
                        } else {
                            // printf("Mode field is empty\n");
                        }
                        break;
                    case 2: // Fix Mode
                        if (!empty_field) {
                            gps.fix_mode = atoi(current_idx);
                            #ifdef DEBUG_GSA
                                ESP_LOGI(TAG,"fix mode: %d", gps.fix_mode);
                            #endif
                            // printf("Fix Mode: %d\n", gps.fix_mode);
                        } else {
                            // printf("Fix Mode field is empty\n");
                        }
                        break;
                    case 15: // PDOP
                        if (!empty_field) {
                            gps.dop_p = atof(current_idx);
                            #ifdef DEBUG_GSA
                                ESP_LOGI(TAG,"PDOP: %f", gps.dop_p);
                            #endif
                        } else {
                            // printf("PDOP field is empty\n");
                        }
                        break;
                    case 16: // HDOP
                        if (!empty_field) {
                            gps.dop_h = atof(current_idx);
                            // printf("HDOP: %.2f\n", gps.dop_h);
                            #ifdef DEBUG_GSA
                                ESP_LOGI(TAG,"HDOP: %f", gps.dop_h);
                            #endif
                        } else {
                            // printf("HDOP field is empty\n");
                        }
                        break;
                    case 17: // VDOP
                        if (!empty_field) {
                            gps.dop_v = atof(current_idx);
                            // printf("VDOP: %.2f\n", gps.dop_v);
                            #ifdef DEBUG_GSA
                                ESP_LOGI(TAG,"VDOP: %f", gps.dop_v);
                            #endif
                        } else {
                            // printf("VDOP field is empty\n");
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

static void parse_rmc(const char *sentence, uint16_t len) {
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
                            parse_time(current_idx, &(gps.tim.hour), &(gps.tim.minute), &(gps.tim.second));
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG,"Time: %d:%d:%f", gps.tim.hour, gps.tim.minute, gps.tim.second);
                            #endif
                        }
                        break;
                    case 2: // Validity
                        if(item_length > 0){
                            gps.valid = (current_idx[0] == 'A') ? true : false;
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG,"Validity status: %d", gps.valid);
                            #endif
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
                                gps.latitude = tmp_latitude * -1;
                            } else if (current_idx[0] == 'N' || current_idx[0] == 'n') {
                                gps.latitude = tmp_latitude;
                            }
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG, "latitude: %f",gps.latitude);
                            #endif
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
                                gps.longitude = tmp_longitude * -1;
                            } else if (current_idx[0] == 'E' || current_idx[0] == 'e') {
                                gps.longitude = tmp_longitude;
                            }
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG, "longitude: %f",gps.longitude);
                            #endif
                        }
                        break;
                    case 7: // Speed (Knots)
                        if (item_length > 0) {
                            gps.speed = atof(current_idx);
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG, "speed: %f",gps.speed);
                            #endif
                        }
                        break;
                    case 8: // Course over ground
                        if (item_length > 0) {
                            gps.cog = atof(current_idx);
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG, "cog: %f",gps.cog);
                            #endif
                        }
                        break;
                    case 9: // Date
                        if (item_length >= 6) {
                            // Assuming date format is DDMMYY
                            parse_date(current_idx, &(gps.date.day), &(gps.date.month), &(gps.date.year));
                            #ifdef DEBUG_RMC
                                ESP_LOGI(TAG, "date: %d/%d/%d", gps.date.day, gps.date.month, gps.date.year);
                            #endif
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


static void parse_vtg(const char *sentence, uint16_t len) {
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
                            gps.speed = atof(current_field);
                            #ifdef DEBUG_VTG
                                ESP_LOGI(TAG, "speed: %f", gps.speed);
                            #endif
                        }
                        break;
                    case 7: // Ground speed in kilometers per hour
                        if (field_length > 0) {
                            // Parse and store the value if it's not empty
                            // Assuming the conversion factor is 1.852
                            gps.speedkmh = atof(current_field) * 1.852;
                            #ifdef DEBUG_VTG
                                ESP_LOGI(TAG,"Speed km/h: %f",gps.speedkmh);
                            #endif

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


void gps_parse(const char *sentence, size_t len)
{
    int i = 1;
    char type[7]; // size greater by 1 to accommodate null terminator
    if (sentence[0] != '$' || len < 3) {
        ESP_LOGI(TAG, "Invalid GPS sentence.\r\n");
        return;
    }
    
    //parse checksum from sentence
    int d=0;
    while(sentence[d] != '*'){
        d++;
    }
    char checksum_str[2] = {sentence[d+1],sentence[d+2]};
    int provided_checksum = strtol(checksum_str, NULL, 16);

    // Perform checksum calculation
    int calculated_checksum = calculate_checksum(sentence);

    if(provided_checksum != calculated_checksum){
        ESP_LOGI(TAG,"CRC error");
        return;
    }

    while (sentence[i] != ',') {
        type[i - 1] = sentence[i]; // Copy character from dtmp to type
        i++;
    }
    type[i - 1] = '\0'; // Null-terminate the string in type
    
    if(strcmp(type, "GPGGA") == 0) {
        #ifdef DEBUG_GGA
            ESP_LOGI(TAG, "%s",sentence);
        #endif
        parse_gga(sentence, len);
        // ESP_LOGI(TAG,"GGA");
    }
    else if(strcmp(type, "GPGSA") == 0){
        #ifdef DEBUG_GSA
            ESP_LOGI(TAG, "%s",sentence);
        #endif
        parse_gsa(sentence, len);
        // ESP_LOGI(TAG,"GSA");
    }
    else if(strcmp(type, "GPGSV") == 0){
        // ESP_LOGI(TAG, "%s",sentence);
        // parse_gsv();
        // ESP_LOGI(TAG,"GSV");
    }
    else if(strcmp(type, "GPRMC") == 0){
        #ifdef DEBUG_RMC
            ESP_LOGI(TAG, "%s",sentence);
        #endif
        parse_rmc(sentence, len);
        // ESP_LOGI(TAG,"RMC");
    }
    else if(strcmp(type, "GPGLL") == 0){
        // parse_gll();
        // ESP_LOGI(TAG,"GLL");
    }
    else if(strcmp(type, "GPVTG") == 0){
        #ifdef DEBUG_VTG
            ESP_LOGI(TAG, "%s",sentence);
        #endif
        parse_vtg(sentence, len);
        // ESP_LOGI(TAG,"VTG");
    }
}

