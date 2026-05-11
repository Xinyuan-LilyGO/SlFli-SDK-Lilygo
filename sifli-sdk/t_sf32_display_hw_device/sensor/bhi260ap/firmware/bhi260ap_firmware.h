#ifndef __BHI260AP_FIRMWARE_H__
#define __BHI260AP_FIRMWARE_H__

#ifdef __cplusplus__
extern "C"
{
#endif

#define BHI260AP 1

#if defined(BHI260AP_aux_BMM150_BMP390_BME688_FLASH)
    #include "./bhi260ap/BHI260AP_aux_BMM150_BMP390_BME688-flash.fw.h"
#elif defined(BHI260AP_aux_BMM150_BMP390_BME688)
    #include "./bhi260ap/BHI260AP_aux_BMM150_BMP390_BME688.fw.h"
#elif defined(BHI260AP_aux_BMM150_FLASH)
    #include "./bhi260ap/BHI260AP_aux_BMM150-flash.fw.h"
#elif defined(BHI260AP_aux_BMM150)
    #include "./bhi260ap/BHI260AP_aux_BMM150.fw.h"
#elif defined(BHI260AP_BME688_FLASH)
    #include "./bhi260ap/BHI260AP_BME688-flash.fw.h"
#elif defined(BHI260AP_BME688)
    #include "./bhi260ap/BHI260AP_BME688.fw.h"
#elif defined(BHI260AP_BMP390_FLASH)
    #include "./bhi260ap/BHI260AP_BMP390-flash.fw.h"
#elif defined(BHI260AP_BMP390)
    #include "./bhi260ap/BHI260AP_BMP390.fw.h"
#elif defined(BHI260AP_TURBO_FLASH)
    #include "./bhi260ap/BHI260AP_TURBO-flash.fw.h"
#elif defined(BHI260AP_TURBO)
    #include "./bhi260ap/BHI260AP_TURBO.fw.h"
#elif defined(BHI260AP_FLASH)
    #include "./bhi260ap/BHI260AP-flash.fw.h"
#elif defined(BHI260AP)
    #include "././bhi260ap/BHI260AP.fw.h"
#elif defined(BHI260AP_KLIO_TURBO_FLASH)
    #include "./bhi260ap_klio/BHI260AP_KLIO_TURBO-flash.fw.h"
#elif defined(BHI260AP_KLIO_FLASH)
    #include "./bhi260ap_klio/BHI260AP_KLIO-flash.fw.h"
#elif defined(BHI260AP_KLIO)
    #include "./bhi260ap_klio/BHI260AP_KLIO.fw.h"
#elif defined(BHI260AP_SWIM_FLASH)
    #include "./bhi260ap_swim/BHI260AP_SWIM-flash.fw.h"
#elif defined(BHI260AP_SWIM)
    #include "./bhi260ap_swim/BHI260AP_SWIM.fw.h"
#else
    #warning "None of the defined conditions were met, so no firmware will be included".
#endif


#ifdef __cplusplus__
}
#endif

#endif