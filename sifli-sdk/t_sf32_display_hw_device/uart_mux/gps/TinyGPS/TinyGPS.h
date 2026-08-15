/*
 * TinyGPS++ - a small GPS library for Arduino providing universal NMEA parsing
 * Based on work by and "distanceBetween" and "courseTo" courtesy of Maarten Lamers.
 * Suggestion to add satellites, courseTo(), and cardinal() by Matt Monson.
 * Location precision improvements suggested by Wayne Holder.
 * Copyright (C) 2008-2024 Mikal Hart
 * All rights reserved.
 *
 * Modified to C for RT-Thread by conversion.
 */

 #ifndef __TINYGPS_H__
 #define __TINYGPS_H__
 
 #include <stdint.h>
 #include <stdbool.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 #define _GPS_VERSION "1.1.0"   // software version of this library
 #define _GPS_MPH_PER_KNOT 1.15077945
 #define _GPS_MPS_PER_KNOT 0.51444444
 #define _GPS_KMPH_PER_KNOT 1.852
 #define _GPS_MILES_PER_METER 0.00062137112
 #define _GPS_KM_PER_METER 0.001
 #define _GPS_FEET_PER_METER 3.2808399
 #define _GPS_MAX_FIELD_SIZE 15
 #define _GPS_EARTH_MEAN_RADIUS 6371009
 
 /* Raw degrees structure */
 typedef struct RawDegrees {
     uint16_t deg;
     uint32_t billionths;
     bool negative;
 } RawDegrees;
 
 /* Forward declarations */
 struct TinyGPSPlus;
 
 /* TinyGPSLocation structure */
 typedef enum {
     GPS_QUALITY_INVALID = '0',
     GPS_QUALITY_GPS = '1',
     GPS_QUALITY_DGPS = '2',
     GPS_QUALITY_PPS = '3',
     GPS_QUALITY_RTK = '4',
     GPS_QUALITY_FLOAT_RTK = '5',
     GPS_QUALITY_ESTIMATED = '6',
     GPS_QUALITY_MANUAL = '7',
     GPS_QUALITY_SIMULATED = '8'
 } TinyGPSLocation_Quality;
 
 typedef enum {
     GPS_MODE_N = 'N',
     GPS_MODE_A = 'A',
     GPS_MODE_D = 'D',
     GPS_MODE_E = 'E'
 } TinyGPSLocation_Mode;
 
 typedef struct TinyGPSLocation {
     bool valid;
     bool updated;
     RawDegrees rawLatData;
     RawDegrees rawLngData;
     RawDegrees rawNewLatData;
     RawDegrees rawNewLngData;
     TinyGPSLocation_Quality fixQuality;
     TinyGPSLocation_Quality newFixQuality;
     TinyGPSLocation_Mode fixMode;
     TinyGPSLocation_Mode newFixMode;
     uint32_t lastCommitTime;
 } TinyGPSLocation;
 
 /* TinyGPSDate structure */
 typedef struct TinyGPSDate {
     bool valid;
     bool updated;
     uint32_t date;
     uint32_t newDate;
     uint32_t lastCommitTime;
 } TinyGPSDate;
 
 /* TinyGPSTime structure */
 typedef struct TinyGPSTime {
     bool valid;
     bool updated;
     uint32_t time;
     uint32_t newTime;
     uint32_t lastCommitTime;
 } TinyGPSTime;
 
 /* TinyGPSDecimal structure */
 typedef struct TinyGPSDecimal {
     bool valid;
     bool updated;
     uint32_t lastCommitTime;
     int32_t val;
     int32_t newval;
 } TinyGPSDecimal;
 
 /* TinyGPSInteger structure */
 typedef struct TinyGPSInteger {
     bool valid;
     bool updated;
     uint32_t lastCommitTime;
     uint32_t val;
     uint32_t newval;
 } TinyGPSInteger;
 
 /* TinyGPSSpeed (inherits from TinyGPSDecimal) */
 typedef struct TinyGPSSpeed {
     TinyGPSDecimal base;
 } TinyGPSSpeed;
 
 /* TinyGPSCourse (inherits from TinyGPSDecimal) */
 typedef struct TinyGPSCourse {
     TinyGPSDecimal base;
 } TinyGPSCourse;
 
 /* TinyGPSAltitude (inherits from TinyGPSDecimal) */
 typedef struct TinyGPSAltitude {
     TinyGPSDecimal base;
 } TinyGPSAltitude;
 
 /* TinyGPSHDOP (inherits from TinyGPSDecimal) */
 typedef struct TinyGPSHDOP {
     TinyGPSDecimal base;
 } TinyGPSHDOP;
 
 /* TinyGPSCustom structure */
 typedef struct TinyGPSCustom {
     char stagingBuffer[_GPS_MAX_FIELD_SIZE + 1];
     char buffer[_GPS_MAX_FIELD_SIZE + 1];
     uint32_t lastCommitTime;
     bool valid;
     bool updated;
     const char *sentenceName;
     int termNumber;
     struct TinyGPSCustom *next;
 } TinyGPSCustom;
 
 /* Main TinyGPSPlus structure */
 typedef struct TinyGPSPlus {
     TinyGPSLocation location;
     TinyGPSDate date;
     TinyGPSTime time;
     TinyGPSSpeed speed;
     TinyGPSCourse course;
     TinyGPSAltitude altitude;
     TinyGPSInteger satellites;
     TinyGPSHDOP hdop;
 
     /* Parsing state variables */
     uint8_t parity;
     bool isChecksumTerm;
     char term[_GPS_MAX_FIELD_SIZE];
     uint8_t curSentenceType;
     uint8_t curTermNumber;
     uint8_t curTermOffset;
     bool sentenceHasFix;
 
     /* Custom element support */
     TinyGPSCustom *customElts;
     TinyGPSCustom *customCandidates;
 
     /* Statistics */
     uint32_t encodedCharCount;
     uint32_t sentencesWithFixCount;
     uint32_t failedChecksumCount;
     uint32_t passedChecksumCount;
 } TinyGPSPlus;
 
 /* Initialization */
 void TinyGPSPlus_init(TinyGPSPlus *gps);
 
 /* Encode one character from GPS */
 bool TinyGPSPlus_encode(TinyGPSPlus *gps, char c);
 
 /* Utility functions */
 int32_t TinyGPSPlus_parseDecimal(const char *term);
 void TinyGPSPlus_parseDegrees(const char *term, RawDegrees *deg);
 double TinyGPSPlus_distanceBetween(double lat1, double lon1, double lat2, double lon2);
 double TinyGPSPlus_courseTo(double lat1, double lon1, double lat2, double lon2);
 const char *TinyGPSPlus_cardinal(double course);
 const char *TinyGPSPlus_libraryVersion(void);
 
 /* Statistics */
 uint32_t TinyGPSPlus_charsProcessed(const TinyGPSPlus *gps);
 uint32_t TinyGPSPlus_sentencesWithFix(const TinyGPSPlus *gps);
 uint32_t TinyGPSPlus_failedChecksum(const TinyGPSPlus *gps);
 uint32_t TinyGPSPlus_passedChecksum(const TinyGPSPlus *gps);
 
 /* Location functions */
 bool TinyGPSLocation_isValid(const TinyGPSLocation *loc);
 bool TinyGPSLocation_isUpdated(const TinyGPSLocation *loc);
 uint32_t TinyGPSLocation_age(const TinyGPSLocation *loc);
 const RawDegrees *TinyGPSLocation_rawLat(TinyGPSLocation *loc);
 const RawDegrees *TinyGPSLocation_rawLng(TinyGPSLocation *loc);
 double TinyGPSLocation_lat(TinyGPSLocation *loc);
 double TinyGPSLocation_lng(TinyGPSLocation *loc);
 TinyGPSLocation_Quality TinyGPSLocation_fixQuality(TinyGPSLocation *loc);
 TinyGPSLocation_Mode TinyGPSLocation_fixMode(TinyGPSLocation *loc);
 
 /* Date functions */
 bool TinyGPSDate_isValid(const TinyGPSDate *date);
 bool TinyGPSDate_isUpdated(const TinyGPSDate *date);
 uint32_t TinyGPSDate_age(const TinyGPSDate *date);
 uint32_t TinyGPSDate_value(TinyGPSDate *date);
 uint16_t TinyGPSDate_year(TinyGPSDate *date);
 uint8_t TinyGPSDate_month(TinyGPSDate *date);
 uint8_t TinyGPSDate_day(TinyGPSDate *date);
 
 /* Time functions */
 bool TinyGPSTime_isValid(const TinyGPSTime *time);
 bool TinyGPSTime_isUpdated(const TinyGPSTime *time);
 uint32_t TinyGPSTime_age(const TinyGPSTime *time);
 uint32_t TinyGPSTime_value(TinyGPSTime *time);
 uint8_t TinyGPSTime_hour(TinyGPSTime *time);
 uint8_t TinyGPSTime_minute(TinyGPSTime *time);
 uint8_t TinyGPSTime_second(TinyGPSTime *time);
 uint8_t TinyGPSTime_centisecond(TinyGPSTime *time);
 
 /* Decimal functions */
 bool TinyGPSDecimal_isValid(const TinyGPSDecimal *dec);
 bool TinyGPSDecimal_isUpdated(const TinyGPSDecimal *dec);
 uint32_t TinyGPSDecimal_age(const TinyGPSDecimal *dec);
 int32_t TinyGPSDecimal_value(TinyGPSDecimal *dec);
 
 /* Integer functions */
 bool TinyGPSInteger_isValid(const TinyGPSInteger *i);
 bool TinyGPSInteger_isUpdated(const TinyGPSInteger *i);
 uint32_t TinyGPSInteger_age(const TinyGPSInteger *i);
 uint32_t TinyGPSInteger_value(TinyGPSInteger *i);
 
 /* Speed functions */
 double TinyGPSSpeed_knots(const TinyGPSSpeed *spd);
 double TinyGPSSpeed_mph(const TinyGPSSpeed *spd);
 double TinyGPSSpeed_mps(const TinyGPSSpeed *spd);
 double TinyGPSSpeed_kmph(const TinyGPSSpeed *spd);
 
 /* Course functions */
 double TinyGPSCourse_deg(const TinyGPSCourse *crs);
 
 /* Altitude functions */
 double TinyGPSAltitude_meters(const TinyGPSAltitude *alt);
 double TinyGPSAltitude_miles(const TinyGPSAltitude *alt);
 double TinyGPSAltitude_kilometers(const TinyGPSAltitude *alt);
 double TinyGPSAltitude_feet(const TinyGPSAltitude *alt);
 
 /* HDOP functions */
 double TinyGPSHDOP_hdop(const TinyGPSHDOP *hdop);
 
 /* Custom element functions */
 void TinyGPSCustom_begin(TinyGPSCustom *custom, TinyGPSPlus *gps, const char *sentenceName, int termNumber);
 bool TinyGPSCustom_isUpdated(const TinyGPSCustom *custom);
 bool TinyGPSCustom_isValid(const TinyGPSCustom *custom);
 uint32_t TinyGPSCustom_age(const TinyGPSCustom *custom);
 const char *TinyGPSCustom_value(TinyGPSCustom *custom);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* __TINYGPS_H__ */