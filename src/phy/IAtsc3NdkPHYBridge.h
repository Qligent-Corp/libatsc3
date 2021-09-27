/*
 * IAtsc3NdkApplicationBridge.h
 *
 *  Created on: Aug 10, 2020
 *      Author: jjustman
 */
#include <string>
#include "IAtsc3NdkPHYClient.h"
#include "IAtsc3NdkPHYClientRFMetrics.h"

using namespace std;

#ifndef LIBATSC3_IATSC3NDKPHYBRIDGE_H
#define LIBATSC3_IATSC3NDKPHYBRIDGE_H

class IAtsc3NdkPHYBridge {
    public:

        virtual void LogMsg(const char *msg) = 0;
        virtual void LogMsg(const std::string &msg) = 0;
        virtual void LogMsgF(const char *fmt, ...) = 0;

        //jjustman-2021-02-04 - for "catastrophic" PHY error reporting to application UI
        virtual void atsc3_notify_phy_error(const char* fmt, ...) = 0;

        //moving to "friend" scope
        virtual void atsc3_update_rf_stats(int32_t tuner_lock,    //1
                                      int32_t rssi_1000,
                                      uint8_t modcod_valid,
                                      uint8_t plp_fec_type,
                                      uint8_t plp_mod,
                                      uint8_t plp_cod,
                                      int32_t nSnr_1000,
                                      uint32_t ber_pre_ldpc_e7,
                                      uint32_t ber_pre_bch_e9,
                                      uint32_t fer_post_bch_e6,
                                      uint8_t demod_lock,
                                      uint8_t signal,
                                      uint8_t plp_any,
                                      uint8_t plp_all) =0; //15

        virtual void atsc3_update_rf_stats_from_atsc3_ndk_phy_client_rf_metrics_t(atsc3_ndk_phy_client_rf_metrics_t* atsc3_ndk_phy_client_rf_metrics) = 0;

        virtual void atsc3_update_rf_bw_stats(uint64_t total_pkts, uint64_t total_bytes, unsigned int total_lmts) = 0;

        virtual void atsc3_update_l1d_time_information(uint8_t l1B_time_info_flag, uint32_t l1D_time_sec, uint16_t l1D_time_msec, uint16_t l1D_time_usec, uint16_t l1D_time_nsec) = 0;

    //application callbacks - mapped to native method for Android
        virtual void setRfPhyStatisticsViewVisible(bool isRfPhyStatisticsVisible) = 0;

        virtual int pinCaptureThreadAsNeeded() = 0;
        virtual int releasePinnedCaptureThreadAsNeeded() = 0;

        virtual int pinStatusThreadAsNeeded() = 0;
        virtual int releasePinnedStatusThreadAsNeeded() = 0;

};

#endif //LIBATSC3_IATSC3NDKPHYBRIDGE_H
