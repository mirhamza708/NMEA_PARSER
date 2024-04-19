#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"


#define GMT 5
#define GPS_MAX_SATELLITES_IN_USE (12)
#define GPS_MAX_SATELLITES_IN_VIEW (16)
#define MAX_SENTENCE_LENGTH 82

#define DEBUG_GGA 1
#define DEBUG_GSA 1
#define DEBUG_RMC 1
#define DEBUG_VTG 1

typedef enum {
    GPS_MODE_INVALID = 1, /*!< Not fixed */
    GPS_MODE_2D,          /*!< 2D GPS */
    GPS_MODE_3D           /*!< 3D GPS */
} gps_fix_mode_t;

typedef enum {
    GPS_FIX_INVALID, /*!< Not fixed */
    GPS_FIX_GPS,     /*!< GPS */
    GPS_FIX_DGPS,    /*!< Differential GPS */
} gps_fix_t;

typedef struct {
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    float second;    /*!< Second */
    uint16_t thousand; /*!< Thousand */
} gps_time_t;

typedef struct {
    uint8_t num;       /*!< Satellite number */
    uint8_t elevation; /*!< Satellite elevation */
    uint16_t azimuth;  /*!< Satellite azimuth */
    uint8_t snr;       /*!< Satellite signal noise ratio */
} gps_satellite_t;

typedef struct {
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    uint16_t year; /*!< Year (start from 2000) */
} gps_date_t;

typedef enum {
    GPS_OKAY,
    GPS_PTR_TO_NULL,
    GPS_INV_SENTENCE,
    GPS_SENTENCE_MISMATCH,
    GPS_CRC_ERROR,
    GPS_MEM_LOW,
} gps_status_t;

typedef struct {
    char current_sentence_type[6];
    float latitude;                                                /*!< Latitude (degrees) */
    float longitude;                                               /*!< Longitude (degrees) */
    float altitude;                                                /*!< Altitude (meters) */
    float geoid_height;                                            /*!< Height above or below the WGS84 ellipsoid (meters) */
    gps_fix_t fix;                                                 /*!< Fix status */
    uint8_t sats_in_use;                                           /*!< Number of satellites in use */
    gps_time_t tim;                                                /*!< time in UTC */
    gps_fix_mode_t fix_mode;                                       /*!< Fix mode */
    char mode;
    uint8_t sats_id_in_use[GPS_MAX_SATELLITES_IN_USE];             /*!< ID list of satellite in use */
    float dop_h;                                                   /*!< Horizontal dilution of precision */
    float dop_p;                                                   /*!< Position dilution of precision  */
    float dop_v;                                                   /*!< Vertical dilution of precision  */
    uint8_t sats_in_view;                                          /*!< Number of satellites in view */
    gps_satellite_t sats_desc_in_view[GPS_MAX_SATELLITES_IN_VIEW]; /*!< Information of satellites in view */
    gps_date_t date;                                               /*!< Fix date */
    bool valid;                                                    /*!< GPS validity */
    float speed;                                                   /*!< Ground speed, unit: knots */
    float speedkmh;                                                /*!< Ground speed, unit: km/h */
    float cog;                                                     /*!< Course over ground */
    float variation;                                               /*!< Magnetic variation */
    gps_status_t status;                                           /*!< gps status */
} gps_t;

gps_t gps_parse(const char *sentence);
