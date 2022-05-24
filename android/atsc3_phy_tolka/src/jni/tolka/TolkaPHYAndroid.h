//
// Created by Jason Justman on 5/24/22.
//


#include <string.h>
#include <jni.h>
#include <thread>
#include <mutex>
#include <semaphore.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <libusb.h>
#endif

using namespace std;

#include <Atsc3LoggingUtils.h>
#include <Atsc3JniEnv.h>
#include <atsc3_utils.h>
#include <atsc3_sl_tlv_demod_type.h>
#include <atsc3_alp_parser.h>

#include <atsc3_core_service_player_bridge.h>


#ifndef ANDROID_ATSC_3_0_SAMPLE_APP_A344_AND_PHY_SUPPORT_TOLKAPHYANDROID_H
#define ANDROID_ATSC_3_0_SAMPLE_APP_A344_AND_PHY_SUPPORT_TOLKAPHYANDROID_H

#include <Atsc3NdkPHYTolkaStaticJniLoader.h>
#include "brUser.h"
#include "IT9300.h"

#define TLV_CIRCULAR_BUFFER_SIZE                 4096000            // TLV circular buffer size, calculated for 2 seconds of user-space interruption at ~15Mbit/sec -> 1.875 * 2 -> 4 MB
#define TLV_CIRCULAR_BUFFER_MIN_PROCESS_SIZE    (16 * 1024 * 2)         //CircularBuffer pending data size threshold for TLV depacketization processing, pinned at 8KB to match SL4000 ALP buffer
#define TLV_CIRCULAR_BUFFER_PROCESS_BLOCK_SIZE  (16 * 1024 * 2)    //CircularBuffer block read size for depacketization callback processing ~ 65KB



#include "CircularBuffer.h"

//#define TOLKA_USB_ENDPOINT_RX 1
//#define TOLKA_USB_ENDPOINT_TX 2
//
//#define TOLKA_USB_ENDPOINT_RX_TS 4

int TOLKA_USB_ENDPOINT_RX = -1;
int TOLKA_USB_ENDPOINT_TX = -1;
int TOLKA_USB_ENDPOINT_RX_TS = -1;


class TolkaPHYAndroid : public IAtsc3NdkPHYClient {

public:
    static mutex CS_global_mutex;

    static libusb_device_handle* Libusb_device_handle;
    //jjustman-2022-05-24 - TODO: add libusb_device_handle for Endeavour ctx instance
    Endeavour endeavour;

    TolkaPHYAndroid(JNIEnv* env, jobject jni_instance);

    virtual int  init()       override;
    virtual int  run()        override;
    virtual bool is_running() override;
    virtual int  stop()       override;
    virtual int  deinit()     override;

    virtual string get_sdk_version()  override;
    virtual string get_firmware_version() override;

    virtual int  download_bootloader_firmware(int fd, int device_type, string devicePath) override;
    virtual int  open(int fd, int device_type, string devicePath)   override;
    virtual int  tune(int freqKhz, int single_plp) override;
    virtual int  listen_plps(vector<uint8_t> plps) override;

    virtual ~TolkaPHYAndroid();

    static void RxDataCallback(unsigned char *data, long len);

    //friend functions for brUser.cpp impls


    static jlong busTx(Dword bufferLength, Byte* buffer);
    static jlong busRx(Dword bufferLength, Byte* buffer);

    int RxThread();

    static void NotifyPlpSelectionChangeCallback(vector<uint8_t> plps, void* context);

    //jjustman-2020-08-23 - moving to public for now..
    uint64_t alp_completed_packets_parsed;
    uint64_t alp_total_bytes;
    uint64_t alp_total_LMTs_recv;

    mutex SL_I2C_command_mutex;
    mutex SL_PlpConfigParams_mutex;

    //jjustman-2021-06-07 - from demod_start
    string                  demodVersion;
//
//    SL_AfeIfConfigParams_t          afeInfo;
//    SL_OutIfConfigParams_t          outPutInfo;
//    SL_IQOffsetCorrectionParams_t   iqOffSetCorrection = { 1.0, 1.0, 0.0, 0.0} ;
//
//    SL_ExtLnaConfigParams_t         lnaInfo = {.lnaMode = SL_EXT_LNA_CFG_MODE_NOT_PRESENT, .lnaGpioNum=0 };
//
//    SL_DemodStd_t                   demodStandard = { SL_DEMODSTD_ATSC3_0 };
//
//    //jjustman-2021-03-03   this is expected to be always accurate, when using, be sure to acquire SL_plpConfigParams_mutex, and SL_I2c_command_mutex, if necessary
//    SL_Atsc3p0ConfigParams_t        atsc3ConfigParams;
//
//    //status thread details - use statusMetricsResetFromContextChange to initalize or to reset when tune() is completed or when PLP selection has changed
//    SL_TunerSignalInfo_t    tunerInfo;
//    SL_DemodLockStatus_t    demodLockStatus;
//    uint                    cpuStatus = 0;
//
//    //jjustman-2021-03-03 - NOTE: the following _Diag's are only polled after acqusition of the relevant RF/L1B/L1D lock, and must be 'cleared' when the tuner is re-tuned
//    SL_Atsc3p0Perf_Diag_t   perfDiag = { 0 };
//    SL_Atsc3p0Bsr_Diag_t    bsrDiag = { 0 };
//    SL_Atsc3p0L1B_Diag_t    l1bDiag = { 0 };
//    SL_Atsc3p0L1D_Diag_t    l1dDiag = { 0 };


    //jjustman-2021-03-02 - don't use this method...
    void dump_plp_list();



protected:
    void pinProducerThreadAsNeeded() override;
    void releasePinnedProducerThreadAsNeeded() override;
    Atsc3JniEnv* producerJniEnv = nullptr;

    void pinConsumerThreadAsNeeded() override;
    void releasePinnedConsumerThreadAsNeeded() override;
    Atsc3JniEnv* consumerJniEnv = nullptr;

    void pinStatusThreadAsNeeded() override;
    void releasePinnedStatusThreadAsNeeded() override;
    Atsc3JniEnv* statusJniEnv = nullptr;

    JNIEnv* env = nullptr;
    jobject jni_instance_globalRef = nullptr;

private:

    int libusb_fd = -1;



    //sanjay
    bool kailash_3_rssi = false;

    //jjustman-2021-10-24 - super-hacky workaround for preboot firmware d/l and proper device type open on re-enumeration call for now..
    static int Last_download_bootloader_firmware_device_id;
    static int Last_tune_freq;

    int slUnit = -1;
    int tUnit = -1;
    int slCmdIfFailureCount = 0;

//
//    SL_PlatFormConfigParams_t getPlfConfig = SL_PLATFORM_CONFIG_PARAMS_NULL_INITIALIZER;
//    SL_PlatFormConfigParams_t sPlfConfig   = SL_PLATFORM_CONFIG_PARAMS_NULL_INITIALIZER;
//
//    SL_CmdControlIf_t         cmdIf;
//    SL_BbCapture_t            getbbValue = BB_CAPTURE_DISABLE;
//
//    SL_LogResult_t            lres;
//    SL_Result_t               slres;
//    SL_TunerResult_t          tres;
//    SL_ConfigResult_t         cres;
//    SL_UtilsResult_t          utilsres;
    int                       demodStartStatus = 0;

    unsigned long long        llsPlpInfo = 0;
    unsigned long long        llsPlpMask = 0x1;
    int                       plpInfoVal = 0, plpllscount = 0;

    int                       last_l1bTimeInfoFlag = -1;
    uint64_t                  last_l1dTimeNs_value = 0;
//
//    SL_DemodConfigInfo_t cfgInfo;
//
//    SL_TunerConfig_t tunerCfg;
//    SL_TunerConfig_t tunerGetCfg;
//
//    //jjustman-2021-11-09 - set "default" tunerIQDcOffset values here, overwritten as needed in hw/device specific configurations
//    SL_TunerDcOffSet_t tunerIQDcOffSet = { 15, 14 };

    //uses      pinProducerThreadAsNeeded
    int         captureThread();
    std::thread captureThreadHandle;
    bool        captureThreadShouldRun = false;
    bool        captureThreadIsRunning = false;

    //uses      pinConsumerThreadAsNeeded
    int         processThread();
    std::thread processThreadHandle;
    bool        processThreadShouldRun = false;
    bool        processThreadIsRunning = false;
    int         processTLVFromCallbackInvocationCount = 0;

    //uses      pinStatusThreadAsNeeded
    int         statusThread();
    std::thread statusThreadHandle;
    bool        statusThreadShouldRun = false;
    bool        statusThreadIsRunning = false;

    void        statusMetricsResetFromTuneChange();
    void        statusMetricsResetFromPLPListenChange();

    void        resetProcessThreadStatistics();

    //if this is our first loop after a Tune() command has completed, dump SL_DemodGetConfiguration
    bool        statusThreadFirstLoopAfterTuneComplete = false;

    //if this is our first loop after a Tune() command has completed AND...

    // we have RF_DemodLock, then get BSR_Diag
    bool        statusThreadFirstLoopAfterTuneComplete_HasBootstrapLock_for_BSR_Diag = false;

    // we have L1B_DemodLock, then get full L1B_Diag
    bool        statusThreadFirstLoopAfterTuneComplete_HasL1B_DemodLock_for_L1B_Diag = false;

    // we have L1D_DemodLock, then get full L1D diag data
    bool        statusThreadFirstLoopAfterTuneComplete_HasL1D_DemodLock_for_L1D_Diag = false;

    //hack
    static CircularBuffer cb;
    static mutex CircularBufferMutex;

    //jjustman-2021-01-19 - used for when we are in a tuning operation and may have in-flight async RxDataCallbacks fired,
    //          if set to true, we should discard the TLV payload in RxDataCallback
    static atomic_bool cb_should_discard;

    //jjustman-2021-03-02 - expanded for runtime implementation support between FX3(aa/bb) and MarkONE,
    // current configuration logic is invoked from int TolkaPHYAndroid::open(int fd, string device_path)
//    SL_ConfigResult_t configPlatformParams_autodetect(int device_type, string device_path);
//
//    SL_ConfigResult_t configPlatformParams_aa_fx3();
//    SL_ConfigResult_t configPlatformParams_aa_markone();
//    SL_ConfigResult_t configPlatformParams_kailash_3_bb_fx3();
//    SL_ConfigResult_t configPlatformParams_yoga_bb_fx3();
//    SL_ConfigResult_t configPlatformParams_bb_markone();

//
//    void printToConsolePlfConfiguration(SL_PlatFormConfigParams_t cfgInfo);
//    void printToConsoleDemodConfiguration(SL_DemodConfigInfo_t cfgInfo);
//
//    void printToConsoleI2cError(const char* methodName, SL_I2cResult_t err);
//    void printToConsoleTunerError(const char* methodName, SL_TunerResult_t err);
//    void printToConsoleDemodError(const char* methodName, SL_Result_t err);

    void handleCmdIfFailure(void);


    void processTLVFromCallback();
    char processDataCircularBufferForCallback[TLV_CIRCULAR_BUFFER_PROCESS_BLOCK_SIZE];

    void                    allocate_atsc3_sl_tlv_block();

    recursive_mutex         atsc3_sl_tlv_block_mutex;   //both atsc3_sl_tlv_block and atsc3_sl_tlv_payload are guarded by atsc3_sl_tlv_block_mutex
    block_t*                atsc3_sl_tlv_block = NULL;
    atsc3_sl_tlv_payload_t* atsc3_sl_tlv_payload = NULL;



    //jjustman-2021-02-04 - global error flag if i2c txn fails, usually due to demod crash
//    static SL_Result_t      global_sl_result_error_flag;
//    static SL_I2cResult_t   global_sl_i2c_result_error_flag;

    //jjustman-2021-06-07 # 11798: compute global/l1b/l1d/plpN SNR metrics
    double compute_snr(int snr_linear_scale);

    //jjustman-2021-08-31 - testing for l1d time info diagnostics
//    void printToConsoleAtsc3L1dDiagnostics(SL_Atsc3p0L1D_Diag_t diag);

};

#define _TOLKA_PHY_ANDROID_ERROR_NOTIFY_BRIDGE_INSTANCE(method, message, cmd_res) \
    if(atsc3_ndk_phy_bridge_get_instance()) { \
        atsc3_ndk_phy_bridge_get_instance()->atsc3_notify_phy_error("TolkaPHYAndroid::%s - ERROR: %s, global_sl_res: %d, global_sl_i2c_res: %d, cmd res: %d", \
        method, message, global_sl_result_error_flag, global_sl_i2c_result_error_flag, cmd_res); \
    } \
    __LIBATSC3_TIMESTAMP_ERROR("TolkaPHYAndroid::%s - ERROR: %s, global_sl_res: %d, global_sl_i2c_res: %d, cmd_res: %d", \
        method, message, global_sl_result_error_flag, global_sl_i2c_result_error_flag, cmd_res);


#define _TOLKA_PHY_ANDROID_ERROR(...)   __LIBATSC3_TIMESTAMP_ERROR(__VA_ARGS__);
#define _TOLKA_PHY_ANDROID_WARN(...)   	__LIBATSC3_TIMESTAMP_WARN(__VA_ARGS__);
#define _TOLKA_PHY_ANDROID_INFO(...)    __LIBATSC3_TIMESTAMP_INFO(__VA_ARGS__);
#define _TOLKA_PHY_ANDROID_DEBUG(...)   if(_TOLKA_PHY_ANDROID_DEBUG_ENABLED) { __LIBATSC3_TIMESTAMP_DEBUG(__VA_ARGS__); }
#define _TOLKA_PHY_ANDROID_TRACE(...)   if(_TOLKA_PHY_ANDROID_TRACE_ENABLED) { __LIBATSC3_TIMESTAMP_TRACE(__VA_ARGS__); }


#endif //ANDROID_ATSC_3_0_SAMPLE_APP_A344_AND_PHY_SUPPORT_TOLKAPHYANDROID_H
