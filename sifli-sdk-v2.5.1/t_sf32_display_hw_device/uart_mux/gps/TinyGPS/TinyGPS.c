/*
 * TinyGPS++ - a small GPS library for Arduino providing universal NMEA parsing
 * Based on work by and "distanceBetween" and "courseTo" courtesy of Maarten
 * Lamers. Suggestion to add satellites, courseTo(), and cardinal() by Matt
 * Monson. Location precision improvements suggested by Wayne Holder. Copyright
 * (C) 2008-2024 Mikal Hart All rights reserved.
 *
 * Modified to C for RT-Thread by conversion.
 */

#include "tinygps.h"
#include <ctype.h>
#include <math.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

/* RT-Thread millis function (assuming tick period = 1ms) */
static unsigned long millis(void)
{
    return rt_tick_get_millisecond();
}

/* Helper macros */
#define COMBINE(sentence_type, term_number)                                    \
    (((unsigned)(sentence_type) << 5) | term_number)

/* Internal functions */
static int fromHex(char a);
static bool endOfTermHandler(TinyGPSPlus *gps);

/*------------------------------------------------------------*/
/* Public functions                                            */
/*------------------------------------------------------------*/

void TinyGPSPlus_init(TinyGPSPlus *gps)
{
    memset(gps, 0, sizeof(TinyGPSPlus));
    gps->term[0] = '\0';
    gps->location.valid = false;
    gps->location.updated = false;
    gps->location.fixQuality = GPS_QUALITY_INVALID;
    gps->location.fixMode = GPS_MODE_N;
    gps->date.valid = false;
    gps->date.updated = false;
    gps->time.valid = false;
    gps->time.updated = false;
    gps->speed.base.valid = false;
    gps->speed.base.updated = false;
    gps->course.base.valid = false;
    gps->course.base.updated = false;
    gps->altitude.base.valid = false;
    gps->altitude.base.updated = false;
    gps->satellites.valid = false;
    gps->satellites.updated = false;
    gps->hdop.base.valid = false;
    gps->hdop.base.updated = false;
    gps->customElts = NULL;
    gps->customCandidates = NULL;
}

bool TinyGPSPlus_encode(TinyGPSPlus *gps, char c)
{
    gps->encodedCharCount++;
    switch (c)
    {
    case ',': // term terminators
        gps->parity ^= (uint8_t)c;
        // fall through
    case '\r':
    case '\n':
    case '*':
    {
        bool isValidSentence = false;
        if (gps->curTermOffset < sizeof(gps->term))
        {
            gps->term[gps->curTermOffset] = 0;
            isValidSentence = endOfTermHandler(gps);
        }
        gps->curTermNumber++;
        gps->curTermOffset = 0;
        gps->isChecksumTerm = (c == '*');
        return isValidSentence;
    }
    case '$': // sentence begin
        gps->curTermNumber = gps->curTermOffset = 0;
        gps->parity = 0;
        gps->curSentenceType = 2; // GPS_SENTENCE_OTHER
        gps->isChecksumTerm = false;
        gps->sentenceHasFix = false;
        return false;
    default: // ordinary characters
        if (gps->curTermOffset < sizeof(gps->term) - 1)
            gps->term[gps->curTermOffset++] = c;
        if (!gps->isChecksumTerm)
            gps->parity ^= c;
        return false;
    }
    return false;
}

/*------------------------------------------------------------*/
/* Internal utilities                                           */
/*------------------------------------------------------------*/
static int fromHex(char a)
{
    if (a >= 'A' && a <= 'F')
        return a - 'A' + 10;
    else if (a >= 'a' && a <= 'f')
        return a - 'a' + 10;
    else
        return a - '0';
}

int32_t TinyGPSPlus_parseDecimal(const char *term)
{
    bool negative = (*term == '-');
    if (negative)
        term++;
    int32_t ret = 100 * (int32_t)atol(term);
    while (isdigit(*term))
        term++;
    if (*term == '.' && isdigit(term[1]))
    {
        ret += 10 * (term[1] - '0');
        if (isdigit(term[2]))
            ret += term[2] - '0';
    }
    return negative ? -ret : ret;
}

void TinyGPSPlus_parseDegrees(const char *term, RawDegrees *deg)
{
    uint32_t leftOfDecimal = (uint32_t)atol(term);
    uint16_t minutes = (uint16_t)(leftOfDecimal % 100);
    uint32_t multiplier = 10000000UL;
    uint32_t tenMillionthsOfMinutes = minutes * multiplier;

    deg->deg = (int16_t)(leftOfDecimal / 100);

    while (isdigit(*term))
        ++term;

    if (*term == '.')
        while (isdigit(*++term))
        {
            multiplier /= 10;
            tenMillionthsOfMinutes += (*term - '0') * multiplier;
        }

    deg->billionths = (5 * tenMillionthsOfMinutes + 1) / 3;
    deg->negative = false;
}

static bool endOfTermHandler(TinyGPSPlus *gps)
{
    // If it's the checksum term, and the checksum checks out, commit
    if (gps->isChecksumTerm)
    {
        uint8_t checksum = 16 * fromHex(gps->term[0]) + fromHex(gps->term[1]);
        if (checksum == gps->parity)
        {
            gps->passedChecksumCount++;
            if (gps->sentenceHasFix)
                gps->sentencesWithFixCount++;

            switch (gps->curSentenceType)
            {
            case 1: // GPS_SENTENCE_RMC
                // commit date
                gps->date.date = gps->date.newDate;
                gps->date.updated = true;
                gps->date.valid = true;
                gps->date.lastCommitTime = millis();
                // commit time
                gps->time.time = gps->time.newTime;
                gps->time.updated = true;
                gps->time.valid = true;
                gps->time.lastCommitTime = millis();
                if (gps->sentenceHasFix)
                {
                    // commit location
                    gps->location.rawLatData = gps->location.rawNewLatData;
                    gps->location.rawLngData = gps->location.rawNewLngData;
                    gps->location.fixMode = gps->location.newFixMode;
                    gps->location.updated = true;
                    gps->location.valid = true;
                    gps->location.lastCommitTime = millis();
                    // commit speed
                    gps->speed.base.val = gps->speed.base.newval;
                    gps->speed.base.updated = true;
                    gps->speed.base.valid = true;
                    gps->speed.base.lastCommitTime = millis();
                    // commit course
                    gps->course.base.val = gps->course.base.newval;
                    gps->course.base.updated = true;
                    gps->course.base.valid = true;
                    gps->course.base.lastCommitTime = millis();
                }
                break;
            case 0: // GPS_SENTENCE_GGA
                // commit time
                gps->time.time = gps->time.newTime;
                gps->time.updated = true;
                gps->time.valid = true;
                gps->time.lastCommitTime = millis();
                if (gps->sentenceHasFix)
                {
                    // commit location
                    gps->location.rawLatData = gps->location.rawNewLatData;
                    gps->location.rawLngData = gps->location.rawNewLngData;
                    gps->location.fixQuality = gps->location.newFixQuality;
                    gps->location.updated = true;
                    gps->location.valid = true;
                    gps->location.lastCommitTime = millis();
                    // commit altitude
                    gps->altitude.base.val = gps->altitude.base.newval;
                    gps->altitude.base.updated = true;
                    gps->altitude.base.valid = true;
                    gps->altitude.base.lastCommitTime = millis();
                }
                // commit satellites
                gps->satellites.val = gps->satellites.newval;
                gps->satellites.updated = true;
                gps->satellites.valid = true;
                gps->satellites.lastCommitTime = millis();
                // commit hdop
                gps->hdop.base.val = gps->hdop.base.newval;
                gps->hdop.base.updated = true;
                gps->hdop.base.valid = true;
                gps->hdop.base.lastCommitTime = millis();
                break;
            }

            // Commit custom listeners
            for (TinyGPSCustom *p = gps->customCandidates;
                 p != NULL && strcmp(p->sentenceName,
                                     gps->customCandidates->sentenceName) == 0;
                 p = p->next)
            {
                strcpy(p->buffer, p->stagingBuffer);
                p->lastCommitTime = millis();
                p->valid = true;
                p->updated = true;
            }
            return true;
        }
        else
        {
            gps->failedChecksumCount++;
        }
        return false;
    }

    // The first term determines the sentence type
    if (gps->curTermNumber == 0)
    {
        if (gps->term[0] == 'G' && strchr("PNABL", gps->term[1]) != NULL &&
            !strcmp(gps->term + 2, "RMC"))
            gps->curSentenceType = 1; // GPS_SENTENCE_RMC
        else if (gps->term[0] == 'G' && strchr("PNABL", gps->term[1]) != NULL &&
                 !strcmp(gps->term + 2, "GGA"))
            gps->curSentenceType = 0; // GPS_SENTENCE_GGA
        else
            gps->curSentenceType = 2; // GPS_SENTENCE_OTHER

        // Any custom candidates of this sentence type?
        for (gps->customCandidates = gps->customElts;
             gps->customCandidates != NULL &&
             strcmp(gps->customCandidates->sentenceName, gps->term) < 0;
             gps->customCandidates = gps->customCandidates->next)
            ;
        if (gps->customCandidates != NULL &&
            strcmp(gps->customCandidates->sentenceName, gps->term) > 0)
            gps->customCandidates = NULL;

        return false;
    }

    if (gps->curSentenceType != 2 && gps->term[0])
    {
        uint8_t type = gps->curSentenceType;
        uint8_t termNum = gps->curTermNumber;

        switch (COMBINE(type, termNum))
        {
        case COMBINE(1, 1): // RMC time
        case COMBINE(0, 1): // GGA time
            gps->time.newTime = (uint32_t)TinyGPSPlus_parseDecimal(gps->term);
            break;
        case COMBINE(1, 2): // RMC validity
            gps->sentenceHasFix = (gps->term[0] == 'A');
            break;
        case COMBINE(1, 3): // RMC latitude
        case COMBINE(0, 2): // GGA latitude
            TinyGPSPlus_parseDegrees(gps->term, &gps->location.rawNewLatData);
            break;
        case COMBINE(1, 4): // RMC N/S
        case COMBINE(0, 3): // GGA N/S
            gps->location.rawNewLatData.negative = (gps->term[0] == 'S');
            break;
        case COMBINE(1, 5): // RMC longitude
        case COMBINE(0, 4): // GGA longitude
            TinyGPSPlus_parseDegrees(gps->term, &gps->location.rawNewLngData);
            break;
        case COMBINE(1, 6): // RMC E/W
        case COMBINE(0, 5): // GGA E/W
            gps->location.rawNewLngData.negative = (gps->term[0] == 'W');
            break;
        case COMBINE(1, 7): // RMC speed
            gps->speed.base.newval = TinyGPSPlus_parseDecimal(gps->term);
            break;
        case COMBINE(1, 8): // RMC course
            gps->course.base.newval = TinyGPSPlus_parseDecimal(gps->term);
            break;
        case COMBINE(1, 9): // RMC date
            gps->date.newDate = atol(gps->term);
            break;
        case COMBINE(0, 6): // GGA fix quality
            gps->sentenceHasFix = (gps->term[0] > '0');
            gps->location.newFixQuality = (TinyGPSLocation_Quality)gps->term[0];
            break;
        case COMBINE(0, 7): // GGA satellites
            gps->satellites.newval = atol(gps->term);
            break;
        case COMBINE(0, 8): // GGA HDOP
            gps->hdop.base.newval = TinyGPSPlus_parseDecimal(gps->term);
            break;
        case COMBINE(0, 9): // GGA altitude
            gps->altitude.base.newval = TinyGPSPlus_parseDecimal(gps->term);
            break;
        case COMBINE(1, 12): // RMC mode
            gps->location.newFixMode = (TinyGPSLocation_Mode)gps->term[0];
            break;
        }
    }

    // Set custom values as needed
    for (TinyGPSCustom *p = gps->customCandidates;
         p != NULL &&
         strcmp(p->sentenceName, gps->customCandidates->sentenceName) == 0 &&
         p->termNumber <= gps->curTermNumber;
         p = p->next)
    {
        if (p->termNumber == gps->curTermNumber)
        {
            strncpy(p->stagingBuffer, gps->term, sizeof(p->stagingBuffer) - 1);
            p->stagingBuffer[sizeof(p->stagingBuffer) - 1] = '\0';
        }
    }

    return false;
}

/*------------------------------------------------------------*/
/* Static geometry functions                                   */
/*------------------------------------------------------------*/
double TinyGPSPlus_distanceBetween(double lat1, double lon1, double lat2,
                                   double lon2)
{
    double delta = (lon1 - lon2) * 3.141592653589793 / 180.0;
    double sdlong = sin(delta);
    double cdlong = cos(delta);
    lat1 = lat1 * 3.141592653589793 / 180.0;
    lat2 = lat2 * 3.141592653589793 / 180.0;
    double slat1 = sin(lat1);
    double clat1 = cos(lat1);
    double slat2 = sin(lat2);
    double clat2 = cos(lat2);
    delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
    delta = delta * delta;
    delta += (clat2 * sdlong) * (clat2 * sdlong);
    delta = sqrt(delta);
    double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
    delta = atan2(delta, denom);
    return delta * _GPS_EARTH_MEAN_RADIUS;
}

double TinyGPSPlus_courseTo(double lat1, double lon1, double lat2, double lon2)
{
    double dlon = (lon2 - lon1) * 3.141592653589793 / 180.0;
    lat1 = lat1 * 3.141592653589793 / 180.0;
    lat2 = lat2 * 3.141592653589793 / 180.0;
    double a1 = sin(dlon) * cos(lat2);
    double a2 = sin(lat1) * cos(lat2) * cos(dlon);
    a2 = cos(lat1) * sin(lat2) - a2;
    a2 = atan2(a1, a2);
    if (a2 < 0.0)
        a2 += 2.0 * 3.141592653589793;
    return a2 * 180.0 / 3.141592653589793;
}

const char *TinyGPSPlus_cardinal(double course)
{
    static const char *directions[] = {"N",  "NNE", "NE", "ENE", "E",  "ESE",
                                       "SE", "SSE", "S",  "SSW", "SW", "WSW",
                                       "W",  "WNW", "NW", "NNW"};
    int direction = (int)((course + 11.25f) / 22.5f);
    return directions[direction % 16];
}

const char *TinyGPSPlus_libraryVersion(void)
{
    return _GPS_VERSION;
}

/*------------------------------------------------------------*/
/* Statistics functions                                        */
/*------------------------------------------------------------*/
uint32_t TinyGPSPlus_charsProcessed(const TinyGPSPlus *gps)
{
    return gps->encodedCharCount;
}
uint32_t TinyGPSPlus_sentencesWithFix(const TinyGPSPlus *gps)
{
    return gps->sentencesWithFixCount;
}
uint32_t TinyGPSPlus_failedChecksum(const TinyGPSPlus *gps)
{
    return gps->failedChecksumCount;
}
uint32_t TinyGPSPlus_passedChecksum(const TinyGPSPlus *gps)
{
    return gps->passedChecksumCount;
}

/*------------------------------------------------------------*/
/* Location functions                                          */
/*------------------------------------------------------------*/
bool TinyGPSLocation_isValid(const TinyGPSLocation *loc)
{
    return loc->valid;
}
bool TinyGPSLocation_isUpdated(const TinyGPSLocation *loc)
{
    return loc->updated;
}
uint32_t TinyGPSLocation_age(const TinyGPSLocation *loc)
{
    return loc->valid ? millis() - loc->lastCommitTime : (uint32_t)-1;
}
const RawDegrees *TinyGPSLocation_rawLat(TinyGPSLocation *loc)
{
    loc->updated = false;
    return &loc->rawLatData;
}
const RawDegrees *TinyGPSLocation_rawLng(TinyGPSLocation *loc)
{
    loc->updated = false;
    return &loc->rawLngData;
}
double TinyGPSLocation_lat(TinyGPSLocation *loc)
{
    loc->updated = false;
    double ret =
        loc->rawLatData.deg + loc->rawLatData.billionths / 1000000000.0;
    return loc->rawLatData.negative ? -ret : ret;
}
double TinyGPSLocation_lng(TinyGPSLocation *loc)
{
    loc->updated = false;
    double ret =
        loc->rawLngData.deg + loc->rawLngData.billionths / 1000000000.0;
    return loc->rawLngData.negative ? -ret : ret;
}
TinyGPSLocation_Quality TinyGPSLocation_fixQuality(TinyGPSLocation *loc)
{
    loc->updated = false;
    return loc->fixQuality;
}
TinyGPSLocation_Mode TinyGPSLocation_fixMode(TinyGPSLocation *loc)
{
    loc->updated = false;
    return loc->fixMode;
}

/*------------------------------------------------------------*/
/* Date functions                                              */
/*------------------------------------------------------------*/
bool TinyGPSDate_isValid(const TinyGPSDate *date)
{
    return date->valid;
}
bool TinyGPSDate_isUpdated(const TinyGPSDate *date)
{
    return date->updated;
}
uint32_t TinyGPSDate_age(const TinyGPSDate *date)
{
    return date->valid ? millis() - date->lastCommitTime : (uint32_t)-1;
}
uint32_t TinyGPSDate_value(TinyGPSDate *date)
{
    date->updated = false;
    return date->date;
}
uint16_t TinyGPSDate_year(TinyGPSDate *date)
{
    date->updated = false;
    uint16_t year = date->date % 100;
    return year + 2000;
}
uint8_t TinyGPSDate_month(TinyGPSDate *date)
{
    date->updated = false;
    return (date->date / 100) % 100;
}
uint8_t TinyGPSDate_day(TinyGPSDate *date)
{
    date->updated = false;
    return date->date / 10000;
}

/*------------------------------------------------------------*/
/* Time functions                                              */
/*------------------------------------------------------------*/
bool TinyGPSTime_isValid(const TinyGPSTime *time)
{
    return time->valid;
}
bool TinyGPSTime_isUpdated(const TinyGPSTime *time)
{
    return time->updated;
}
uint32_t TinyGPSTime_age(const TinyGPSTime *time)
{
    return time->valid ? millis() - time->lastCommitTime : (uint32_t)-1;
}
uint32_t TinyGPSTime_value(TinyGPSTime *time)
{
    time->updated = false;
    return time->time;
}
uint8_t TinyGPSTime_hour(TinyGPSTime *time)
{
    time->updated = false;
    return time->time / 1000000;
}
uint8_t TinyGPSTime_minute(TinyGPSTime *time)
{
    time->updated = false;
    return (time->time / 10000) % 100;
}
uint8_t TinyGPSTime_second(TinyGPSTime *time)
{
    time->updated = false;
    return (time->time / 100) % 100;
}
uint8_t TinyGPSTime_centisecond(TinyGPSTime *time)
{
    time->updated = false;
    return time->time % 100;
}

/*------------------------------------------------------------*/
/* Decimal functions                                           */
/*------------------------------------------------------------*/
bool TinyGPSDecimal_isValid(const TinyGPSDecimal *dec)
{
    return dec->valid;
}
bool TinyGPSDecimal_isUpdated(const TinyGPSDecimal *dec)
{
    return dec->updated;
}
uint32_t TinyGPSDecimal_age(const TinyGPSDecimal *dec)
{
    return dec->valid ? millis() - dec->lastCommitTime : (uint32_t)-1;
}
int32_t TinyGPSDecimal_value(TinyGPSDecimal *dec)
{
    dec->updated = false;
    return dec->val;
}

/*------------------------------------------------------------*/
/* Integer functions                                           */
/*------------------------------------------------------------*/
bool TinyGPSInteger_isValid(const TinyGPSInteger *i)
{
    return i->valid;
}
bool TinyGPSInteger_isUpdated(const TinyGPSInteger *i)
{
    return i->updated;
}
uint32_t TinyGPSInteger_age(const TinyGPSInteger *i)
{
    return i->valid ? millis() - i->lastCommitTime : (uint32_t)-1;
}
uint32_t TinyGPSInteger_value(TinyGPSInteger *i)
{
    i->updated = false;
    return i->val;
}

/*------------------------------------------------------------*/
/* Speed functions                                             */
/*------------------------------------------------------------*/
double TinyGPSSpeed_knots(const TinyGPSSpeed *spd)
{
    return spd->base.val / 100.0;
}
double TinyGPSSpeed_mph(const TinyGPSSpeed *spd)
{
    return _GPS_MPH_PER_KNOT * spd->base.val / 100.0;
}
double TinyGPSSpeed_mps(const TinyGPSSpeed *spd)
{
    return _GPS_MPS_PER_KNOT * spd->base.val / 100.0;
}
double TinyGPSSpeed_kmph(const TinyGPSSpeed *spd)
{
    return _GPS_KMPH_PER_KNOT * spd->base.val / 100.0;
}

/*------------------------------------------------------------*/
/* Course functions                                            */
/*------------------------------------------------------------*/
double TinyGPSCourse_deg(const TinyGPSCourse *crs)
{
    return crs->base.val / 100.0;
}

/*------------------------------------------------------------*/
/* Altitude functions                                          */
/*------------------------------------------------------------*/
double TinyGPSAltitude_meters(const TinyGPSAltitude *alt)
{
    return alt->base.val / 100.0;
}
double TinyGPSAltitude_miles(const TinyGPSAltitude *alt)
{
    return _GPS_MILES_PER_METER * alt->base.val / 100.0;
}
double TinyGPSAltitude_kilometers(const TinyGPSAltitude *alt)
{
    return _GPS_KM_PER_METER * alt->base.val / 100.0;
}
double TinyGPSAltitude_feet(const TinyGPSAltitude *alt)
{
    return _GPS_FEET_PER_METER * alt->base.val / 100.0;
}

/*------------------------------------------------------------*/
/* HDOP functions                                              */
/*------------------------------------------------------------*/
double TinyGPSHDOP_hdop(const TinyGPSHDOP *hdop)
{
    return hdop->base.val / 100.0;
}

/*------------------------------------------------------------*/
/* Custom element functions                                    */
/*------------------------------------------------------------*/
void TinyGPSCustom_begin(TinyGPSCustom *custom, TinyGPSPlus *gps,
                         const char *sentenceName, int termNumber)
{
    custom->lastCommitTime = 0;
    custom->updated = custom->valid = false;
    custom->sentenceName = sentenceName;
    custom->termNumber = termNumber;
    memset(custom->stagingBuffer, 0, sizeof(custom->stagingBuffer));
    memset(custom->buffer, 0, sizeof(custom->buffer));
    custom->next = NULL;

    // Insert into GPS tree
    TinyGPSCustom **ppelt;
    for (ppelt = &gps->customElts; *ppelt != NULL; ppelt = &(*ppelt)->next)
    {
        int cmp = strcmp(sentenceName, (*ppelt)->sentenceName);
        if (cmp < 0 || (cmp == 0 && termNumber < (*ppelt)->termNumber))
            break;
    }
    custom->next = *ppelt;
    *ppelt = custom;
}

bool TinyGPSCustom_isUpdated(const TinyGPSCustom *custom)
{
    return custom->updated;
}
bool TinyGPSCustom_isValid(const TinyGPSCustom *custom)
{
    return custom->valid;
}
uint32_t TinyGPSCustom_age(const TinyGPSCustom *custom)
{
    return custom->valid ? millis() - custom->lastCommitTime : (uint32_t)-1;
}
const char *TinyGPSCustom_value(TinyGPSCustom *custom)
{
    custom->updated = false;
    return custom->buffer;
}