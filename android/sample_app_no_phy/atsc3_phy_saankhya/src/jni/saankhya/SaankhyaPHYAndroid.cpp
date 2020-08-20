//
// Created by Jason Justman on 8/19/20.
//

#include "SaankhyaPHYAndroid.h"
SaankhyaPHYAndroid* saankhyaPHYAndroid = nullptr;

CircularBuffer SaankhyaPHYAndroid::cb = nullptr;

mutex SaankhyaPHYAndroid::CircularBufferMutex;
//atsc3_sl_tlv_block_Mutex
mutex SaankhyaPHYAndroid::atsc3_sl_tlv_block_Mutex;


Atsc3JniEnv* SaankhyaPHYAndroid::Atsc3_Jni_Capture_Thread_Env = NULL;
Atsc3JniEnv* SaankhyaPHYAndroid::Atsc3_Jni_Processing_Thread_Env = NULL;
Atsc3JniEnv* SaankhyaPHYAndroid::Atsc3_Jni_Status_Thread_Env = NULL;


/* jjustman-2020-08-19 - todo - cleanup */
block_t* atsc3_sl_tlv_block = NULL;
CircularBuffer cb = NULL;
atsc3_sl_tlv_payload_t* atsc3_sl_tlv_payload = NULL;


SaankhyaPHYAndroid::SaankhyaPHYAndroid(JNIEnv* env, jobject jni_instance) {
    this->env = env;
    this->jni_instance_globalRef = env->NewGlobalRef(jni_instance);
    this->setRxUdpPacketProcessCallback(atsc3_core_service_bridge_process_packet_from_plp_and_block);
}

SaankhyaPHYAndroid::~SaankhyaPHYAndroid() {
    if(this->env) {
        if(this->jni_instance_globalRef) {
            env->DeleteGlobalRef(this->jni_instance_globalRef);
            this->jni_instance_globalRef = nullptr;
        }
    }
    if(this->producerJniEnv) {
        delete this->producerJniEnv;
    }
    if(this->consumerJniEnv) {
        delete this->producerJniEnv;
    }
}

void SaankhyaPHYAndroid::pinProducerThreadAsNeeded() {
    producerJniEnv = new Atsc3JniEnv(atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
}

void SaankhyaPHYAndroid::releaseProducerThreadAsNeeded() {
    delete producerJniEnv;
    producerJniEnv = nullptr;
}

void SaankhyaPHYAndroid::pinConsumerThreadAsNeeded() {
    consumerJniEnv = new Atsc3JniEnv(atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    if(atsc3_ndk_application_bridge_get_instance()) {
        atsc3_ndk_application_bridge_get_instance()->pinFromRxProcessingThread();
    }
}

void SaankhyaPHYAndroid::releaseConsumerThreadAsNeeded() {
    delete consumerJniEnv;
    consumerJniEnv = nullptr;
}


void SaankhyaPHYAndroid::resetProcessThreadStatistics() {
    alp_completed_packets_parsed = 0;
    alp_total_bytes = 0;
    alp_total_LMTs_recv = 0;
}


int SaankhyaPHYAndroid::init()
{
    SaankhyaPHYAndroid::configPlatformParams();
    return 0;
}

int SaankhyaPHYAndroid::run()
{
    return 0;
}

bool SaankhyaPHYAndroid::is_running() {
    return 0;
}

int SaankhyaPHYAndroid::stop()
{
    return 0;
}

int SaankhyaPHYAndroid::deinit()
{
    delete this;
    return 0;
}

int SaankhyaPHYAndroid::open(int fd, int bus, int addr)
{
    SL_SetUsbFd(fd);

    SL_I2cResult_t i2cres;

    SL_Result_t slres;
    SL_ConfigResult_t cres;
    SL_TunerResult_t tres;
    SL_UtilsResult_t utilsres;
    SL_TunerConfig_t tunerCfg;
    SL_TunerConfig_t tunerGetCfg;
    SL_TunerSignalInfo_t tunerInfo;
    int swMajorNo, swMinorNo;
    unsigned int cFrequency = 0;
    SL_AfeIfConfigParams_t afeInfo;
    SL_OutIfConfigParams_t outPutInfo;
    SL_IQOffsetCorrectionParams_t iqOffSetCorrection;
    SL_DemodBootStatus_t bootStatus;

    cres = SL_ConfigGetPlatform(&getPlfConfig);
    if (cres == SL_CONFIG_OK)
    {
        printToConsolePlfConfiguration(getPlfConfig);
    }
    else
    {
        SL_Printf("\n ERROR : SL_ConfigGetPlatform Failed ");
        goto ERROR;
    }

    cres = SL_ConfigSetBbCapture(BB_CAPTURE_DISABLE);
    if (cres != SL_CONFIG_OK)
    {
        SL_Printf("\n ERROR : SL_ConfigSetBbCapture Failed ");
        goto ERROR;
    }

    if (getPlfConfig.demodControlIf == SL_DEMOD_CMD_CONTROL_IF_I2C)
    {
        i2cres = SL_I2cInit();
        if (i2cres != SL_I2C_OK)
        {
            _SAANKHYA_PHY_ANDROID_ERROR("ERROR : Error:SL_I2cInit failed Failed");

            SL_Printf("\n Error:SL_I2cInit failed :");
            printToConsoleI2cError(i2cres);
            goto ERROR;
        }
        else
        {
            cmdIf = SL_CMD_CONTROL_IF_I2C;
            printf("atsc3NdkClientSlImpl: setting cmdIf: %d", cmdIf);
        }
    }
    else if (getPlfConfig.demodControlIf == SL_DEMOD_CMD_CONTROL_IF_SDIO)
    {
        SL_Printf("\n Error:SL_SdioInit failed :Not Supported");
        goto ERROR;
    }
    else if (getPlfConfig.demodControlIf == SL_DEMOD_CMD_CONTROL_IF_SPI)
    {
        SL_Printf("\n Error:SL_SpiInit failed :Not Supported");
        goto ERROR;
    }

    /* Demod Config */
    switch (getPlfConfig.boardType)
    {
        case SL_EVB_3000:
            if (getPlfConfig.tunerType == TUNER_NXP)
            {
                afeInfo.spectrum = SL_SPECTRUM_INVERTED;
                afeInfo.iftype = SL_IFTYPE_LIF;
                afeInfo.ifreq = 4.4 + IF_OFFSET;
            }
            else if (getPlfConfig.tunerType == TUNER_SI)
            {
                afeInfo.spectrum = SL_SPECTRUM_NORMAL;
                afeInfo.iftype = SL_IFTYPE_ZIF;
                afeInfo.ifreq = 0.0;
            }
            else
            {
                SL_Printf("\n Invalid Tuner Selection");
            }

            if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_TS)
            {
                outPutInfo.oif = SL_OUTPUTIF_TSSERIAL_LSB_FIRST;
                /* CPLD Reset */
                SL_GpioSetPin(getPlfConfig.cpldResetGpioPin, 0x00);          // Low
                SL_SleepMS(100); // 100ms delay for Toggle
                SL_GpioSetPin(getPlfConfig.cpldResetGpioPin, 0x01);          // High
            }
            else if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_SDIO)
            {
                outPutInfo.oif = SL_OUTPUTIF_SDIO;
            }
            else
            {
                SL_Printf("\n Invalid OutPut Interface Selection");
            }

            afeInfo.iswap = SL_IPOL_SWAP_DISABLE;
            afeInfo.qswap = SL_QPOL_SWAP_DISABLE;
            iqOffSetCorrection.iCoeff1 = 1.0;
            iqOffSetCorrection.qCoeff1 = 1.0;
            iqOffSetCorrection.iCoeff2 = 0.0;
            iqOffSetCorrection.qCoeff2 = 0.0;
            break;

        case SL_EVB_3010:
            if (getPlfConfig.tunerType == TUNER_NXP)
            {
                afeInfo.spectrum = SL_SPECTRUM_INVERTED;
                afeInfo.iftype = SL_IFTYPE_LIF;
                afeInfo.ifreq = 4.4 + IF_OFFSET;
            }
            else if (getPlfConfig.tunerType == TUNER_SI)
            {
                printf("using TUNER_SI, ifreq: 0");
                afeInfo.spectrum = SL_SPECTRUM_NORMAL;
                afeInfo.iftype = SL_IFTYPE_ZIF;
                afeInfo.ifreq = 0.0;
            }
            else
            {
                SL_Printf("\n Invalid Tuner Selection");
            }

            if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_TS)
            {
                outPutInfo.oif = SL_OUTPUTIF_TSPARALLEL_LSB_FIRST;
            }
            else if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_SDIO)
            {
                outPutInfo.oif = SL_OUTPUTIF_SDIO;
            }
            else
            {
                SL_Printf("\n Invalid Output Interface Selection");
            }

            afeInfo.iswap = SL_IPOL_SWAP_DISABLE;
            afeInfo.qswap = SL_QPOL_SWAP_DISABLE;
            iqOffSetCorrection.iCoeff1 = 1.0;
            iqOffSetCorrection.qCoeff1 = 1.0;
            iqOffSetCorrection.iCoeff2 = 0.0;
            iqOffSetCorrection.qCoeff2 = 0.0;
            break;

        case SL_EVB_4000:
            if (getPlfConfig.tunerType == TUNER_SI)
            {
                afeInfo.spectrum = SL_SPECTRUM_NORMAL;
                afeInfo.iftype = SL_IFTYPE_ZIF;
                afeInfo.ifreq = 0.0;
            }
            else
            {
                SL_Printf("\n Invalid Tuner Selection");
            }

            if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_TS)
            {
                outPutInfo.oif = SL_OUTPUTIF_TSSERIAL_LSB_FIRST;
                /* CPLD Reset */
                SL_GpioSetPin(getPlfConfig.cpldResetGpioPin, 0x00); // Low
                SL_SleepMS(100); // 100ms delay for Toggle
                SL_GpioSetPin(getPlfConfig.cpldResetGpioPin, 0x01); // High
            }
            else if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_SDIO)
            {
                outPutInfo.oif = SL_OUTPUTIF_SDIO;
            }
            else
            {
                SL_Printf("\n Invalid Output Interface Selection");
            }

            afeInfo.iswap = SL_IPOL_SWAP_DISABLE;
            afeInfo.qswap = SL_QPOL_SWAP_ENABLE;
            iqOffSetCorrection.iCoeff1 = 1;
            iqOffSetCorrection.qCoeff1 = 1;
            iqOffSetCorrection.iCoeff2 = 0;
            iqOffSetCorrection.qCoeff2 = 0;

            break;

        case SL_KAILASH_DONGLE:
            if (getPlfConfig.tunerType == TUNER_SI)
            {
                printf("using SL_KAILASH with SPECTRUM_NORMAL and ZIF");
                afeInfo.spectrum = SL_SPECTRUM_NORMAL;
                afeInfo.iftype = SL_IFTYPE_ZIF;
                afeInfo.ifreq = 0.0;
            }
            else
            {
                SL_Printf("\n Invalid Tuner Type selected ");
            }

            if (getPlfConfig.demodOutputIf == SL_DEMOD_OUTPUTIF_TS)
            {
                outPutInfo.oif = SL_OUTPUTIF_TSPARALLEL_LSB_FIRST;
            }
            else
            {
                SL_Printf("\n Invalid OutPut Interface Selection");
            }

            afeInfo.iswap = SL_IPOL_SWAP_DISABLE;
            afeInfo.qswap = SL_QPOL_SWAP_ENABLE;
            iqOffSetCorrection.iCoeff1 = (float)1.00724023045574;
            iqOffSetCorrection.qCoeff1 = (float)0.998403791546105;
            iqOffSetCorrection.iCoeff2 = (float)0.0432678874719328;
            iqOffSetCorrection.qCoeff2 = (float)0.0436508327768608;
            break;

        default:
            SL_Printf("\n Invalid Board Type Selected ");
            break;
    }
    afeInfo.iqswap = SL_IQSWAP_DISABLE;
    afeInfo.agcRefValue = 125; //afcRefValue in mV
    outPutInfo.TsoClockInvEnable = SL_TSO_CLK_INV_ON;

    cres = SL_ConfigGetBbCapture(&getbbValue);
    if (cres != SL_CONFIG_OK)
    {
        _SAANKHYA_PHY_ANDROID_ERROR("ERROR : SL_ConfigGetPlatform Failed");

        SL_Printf("\n ERROR : SL_ConfigGetPlatform Failed ");
        goto ERROR;
    }

    if (getbbValue)
    {
        plpInfo.plp0 = 0x00;
    }
    else
    {
        plpInfo.plp0 = 0x00;
    }
    plpInfo.plp1 = 0xFF;
    plpInfo.plp2 = 0xFF;
    plpInfo.plp3 = 0xFF;
    printf("SL_DemodCreateInstance: before");
    slres = SL_DemodCreateInstance(&slUnit);
    if (slres != SL_OK)
    {
        printf("\n Error:SL_DemodCreateInstance: slres: %d", slres);
        SL_Printf("\n Error:SL_DemodCreateInstance :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    //jjustman-2020-07-20 - create thread for libusb_handle_events for context callbacks
    //jjustman-2020-07-29 - disable
    //pthread_create(&pThreadID, NULL, (THREADFUNCPTR)&atsc3NdkClientSlImpl::LibUSB_Handle_Events_Callback, (void*)this);

    SL_Printf("\n Initializing SL Demod..: ");
    printf("SL_DemodInit: before, slUnit: %d, cmdIf: %d", slUnit, cmdIf);
    slres = SL_DemodInit(slUnit, cmdIf, SL_DEMODSTD_ATSC3_0);
    if (slres != SL_OK)
    {
        printf("SL_DemodInit: failed, slres: %d", slres);
        SL_Printf("FAILED");
        SL_Printf("\n Error:SL_DemodInit :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }
    else
    {
        printf("SL_DemodInit: SUCCESS, slres: %d", slres);
        SL_Printf("SUCCESS");
    }

    do
    {
        printf("before SL_DemodGetStatus: slres is: %d", slres);
        slres = SL_DemodGetStatus(slUnit, SL_DEMOD_STATUS_TYPE_BOOT, (SL_DemodBootStatus_t*)&bootStatus);
        printf("SL_DemodGetStatus: slres is: %d", slres);
        if (slres != SL_OK)
        {
            SL_Printf("\n Error:SL_Demod Get Boot Status :");
            printToConsoleDemodError(slres);
        }
        SL_SleepMS(1000);
    } while (bootStatus != SL_DEMOD_BOOT_STATUS_COMPLETE);

    SL_Printf("\n Demod Boot Status      : ");
    printf("\n Demod Boot Status      : ");
    if (bootStatus == SL_DEMOD_BOOT_STATUS_INPROGRESS)
    {
        SL_Printf("%s", "INPROGRESS");
        printf("inprogress");
    }
    else if (bootStatus == SL_DEMOD_BOOT_STATUS_COMPLETE)
    {
        SL_Printf("%s", "COMPLETED");
        printf("COMPLETED");
    }
    else if (bootStatus == SL_DEMOD_BOOT_STATUS_ERROR)
    {
        SL_Printf("%s", "ERROR");
        printf("ERROR");
        goto ERROR;
    }

    slres = SL_DemodConfigure(slUnit, SL_CONFIGTYPE_AFEIF, &afeInfo);
    if (slres != 0)
    {
        SL_Printf("\n Error:SL_DemodConfigure :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    slres = SL_DemodConfigure(slUnit, SL_CONFIG_TYPE_IQ_OFFSET_CORRECTION, &iqOffSetCorrection);
    if (slres != 0)
    {
        SL_Printf("\n Error:SL_DemodConfigure :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    slres = SL_DemodConfigure(slUnit, SL_CONFIGTYPE_OUTPUTIF, &outPutInfo);
    if (slres != 0)
    {
        SL_Printf("\n Error:SL_DemodConfigure :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    slres = SL_DemodConfigPlps(slUnit, &plpInfo);
    if (slres != 0)
    {
        SL_Printf("\n Error:SL_DemodConfigPlps :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    slres = SL_DemodGetSoftwareVersion(slUnit, &swMajorNo, &swMinorNo);
    if (slres == SL_OK)
    {
        SL_Printf("\n Demod SW Version       : %d.%d", swMajorNo, swMinorNo);
        printf("\n Demod SW Version       : %d.%d", swMajorNo, swMinorNo);
    }

    /* Tuner Config */
    tunerCfg.bandwidth = SL_TUNER_BW_6MHZ;
    tunerCfg.std = SL_TUNERSTD_ATSC3_0;

    tres = SL_TunerCreateInstance(&tUnit);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerCreateInstance :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }

    tres = SL_TunerInit(tUnit);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerInit :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }

    tres = SL_TunerConfigure(tUnit, &tunerCfg);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerConfigure :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }

    if (getPlfConfig.boardType == SL_EVB_4000)
    {
        /*
         * Apply tuner IQ offset. Relevant to SITUNE Tuner
         */
        tunerIQDcOffSet.iOffSet = 15;
        tunerIQDcOffSet.qOffSet = 14;

        tres = SL_TunerExSetDcOffSet(tUnit, &tunerIQDcOffSet);
        if (tres != 0)
        {
            SL_Printf("\n Error:SL_TunerExSetDcOffSet :");
            printToConsoleTunerError(tres);
            if (getPlfConfig.tunerType == TUNER_SI)
            {
                goto ERROR;
            }
        }
    }

    printf("OPEN COMPLETE!");
    return 0;

ERROR:

    return -1;
}

int SaankhyaPHYAndroid::tune(int freqKHz, int plpid)
{

    //from atsc3.c ::
    unsigned int cFrequency = 0;
    tres = SL_TunerSetFrequency(tUnit, freqKHz*1000);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerSetFrequency :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }

    tres = SL_TunerGetConfiguration(tUnit, &tunerGetCfg);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerGetConfiguration :");
        printToConsoleTunerError(tres);
        goto ERROR;
    } else {
        if (tunerGetCfg.std == SL_TUNERSTD_ATSC3_0)
        {
            SL_Printf("\n Tuner Config Std       : ATSC3.0");
        }
        else
        {
            SL_Printf("\n Tuner Config Std       : NA");
        }
        switch (tunerGetCfg.bandwidth)
        {
            case SL_TUNER_BW_6MHZ:
                SL_Printf("\n Tuner Config Bandwidth : 6MHz");
                break;

            case SL_TUNER_BW_7MHZ:
                SL_Printf("\n Tuner Config Bandwidth : 7MHz");
                break;

            case SL_TUNER_BW_8MHZ:
                SL_Printf("\n Tuner Config Bandwidth : 8MHz");
                break;

            default:
                SL_Printf("\n Tuner Config Bandwidth : NA");
        }
    }

    tres = SL_TunerGetFrequency(tUnit, &cFrequency);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerGetFrequency :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }
    else
    {
        SL_Printf("\n Tuner Locked Frequency : %dHz", cFrequency);

    }

    tres = SL_TunerGetStatus(tUnit, &tunerInfo);
    if (tres != 0)
    {
        SL_Printf("\n Error:SL_TunerGetStatus :");
        printToConsoleTunerError(tres);
        goto ERROR;
    }
    else
    {
        SL_Printf("\n Tuner Lock status      : ");
        SL_Printf((tunerInfo.status == 1) ? "LOCKED" : "NOT LOCKED");
        SL_Printf("\n Tuner RSSI             : %3.2f dBm", tunerInfo.signalStrength);
    }

    //setup shared memory allocs
    if(!cb) {
        cb = CircularBufferCreate(CB_SIZE);
    }

    if (!atsc3_sl_tlv_block) {
        atsc3_sl_tlv_block = block_Alloc(BUFFER_SIZE);
    }

    printf("creating capture thread, cb buffer size: %d, tlv_block_size: %d",
           CB_SIZE, BUFFER_SIZE);

//    utilsres = SL_CreateThread(&pThread, &ProcessThread);
//    if (utilsres != SL_UTILS_OK)
//    {
//        processFlag = 0;
//        SL_Printf("\n Process Thread launched unsuccessfully");
//        goto ERROR;
//    }
//    else
//    {
//        processFlag = 1;
//        if (SL_SetThreadPriority(&pThread, SL_THREAD_PRIORITY_NORMAL) != SL_UTILS_OK)
//        {
//            SL_Printf("\n Set Thread Priority of Process Thread Failed");
//        }
//    }

    processThreadShouldRun = true;
    pThread = pthread_create(&pThreadID, NULL, (THREADFUNCPTR)&SaankhyaPHYAndroid::ProcessThread, (void*)this);
    if (pThread != 0) {
        //processFlag = 0;
        printf("\n Process Thread launched unsuccessfully");
        goto ERROR;
    } else  {
        //processFlag = 1;
    }


//    utilsres = SL_CreateThread(&cThread, &CaptureThread);
//    if (utilsres != SL_UTILS_OK)
//    {
//        SL_Printf("\n Capture Thread launched unsuccessfully");
//        goto ERROR;
//    }

    SaankhyaPHYAndroid::captureThreadShouldRun = true;
    cThread = pthread_create(&cThreadID, NULL, (THREADFUNCPTR)&SaankhyaPHYAndroid::CaptureThread, (void*)this);
    if (cThread != 0) {
        printf("\n Capture Thread launched unsuccessfully");
        goto ERROR;
    }


    sThread = pthread_create(&sThreadID, NULL, (THREADFUNCPTR) &SaankhyaPHYAndroid::TunerStatusThread, (void*)this);

    while (SL_IsRxDataStarted() != 1)
    {
        SL_SleepMS(100);
    }
    SL_Printf("\n Starting SLDemod: ");

    slres = SL_DemodStart(slUnit);

    if (slres != 0)
    {
        SL_Printf("\n Saankhya Demod Start Failed");
        goto ERROR;
    }
    else
    {
        demodStartStatus = 1;
        SL_Printf("SUCCESS");
        //SL_Printf("\n SL Demod Output Capture: STARTED : sl-tlv.bin");
    }
    SL_SleepMS(1000); // Delay to accomdate set configurations at SL to take effect


    slres = SL_DemodGetLlsPlpList(slUnit, &llsPlpInfo);
    if (slres != SL_OK)
    {
        SL_Printf("\n Error:SL_DemodGetLlsPlpList :");
        printToConsoleDemodError(slres);
        if (slres == SL_ERR_CMD_IF_FAILURE)
        {
            handleCmdIfFailure();
            goto ERROR;
        }
    }

    plpllscount = 0;
    for (int plpIndx = 0; (plpIndx < 64) && (plpllscount < 4); plpIndx++)
    {
        plpInfoVal = ((llsPlpInfo & (llsPlpMask << plpIndx)) == pow(2, plpIndx)) ? 0x01 : 0xFF;
        if (plpInfoVal == 0x01)
        {
            plpllscount++;
            if (plpllscount == 1)
            {
                plpInfo.plp0 = plpIndx;
            }
            else if (plpllscount == 2)
            {
                plpInfo.plp1 = plpIndx;
            }
            else if (plpllscount == 3)
            {
                plpInfo.plp2 = plpIndx;
            }
            else if (plpllscount == 4)
            {
                plpInfo.plp3 = plpIndx;
            }
            else
            {
                plpllscount++;
            }
        }
    }

    if (plpInfo.plp0 == -1)
    {
        plpInfo.plp0 = 0x00;
    }

    plpInfo.plp0 = 0x00;
    slres = SL_DemodConfigPlps(slUnit, &plpInfo);
    if (slres != 0)
    {
        SL_Printf("\n Error:SL_DemodConfigPlps :");
        printToConsoleDemodError(slres);
        goto ERROR;
    }

    slres = SL_DemodGetConfiguration(slUnit, &cfgInfo);
    if (slres != SL_OK)
    {
        SL_Printf("\n Error:SL_DemodGetConfiguration :");
        printToConsoleDemodError(slres);
        if (slres == SL_ERR_CMD_IF_FAILURE)
        {
            handleCmdIfFailure();
            goto ERROR;
        }
    }
    else
    {
        printToConsoleDemodConfiguration(cfgInfo);
    }

    return 0;

 ERROR:
    return -1;
}

int SaankhyaPHYAndroid::listen_plps(vector<uint8_t> plps)
{
    return 0;
}

int SaankhyaPHYAndroid::download_bootloader_firmware(int fd) {
    SL_SetUsbFd(fd);

    SL_I2cResult_t i2cres;

    printf("SL_I2cPreInit - Before");
    i2cres = SL_I2cPreInit();
    printf("SL_I2cPreInit returned: %d", i2cres);

    if (i2cres != SL_I2C_OK)
    {
        if(i2cres == SL_I2C_AWAITING_REENUMERATION) {
            printf("\n INFO:SL_I2cPreInit SL_FX3S_I2C_AWAITING_REENUMERATION");
            //sleep for 2s
            sleep(2);
            return 0;
        } else {
            printf("\n Error:SL_I2cPreInit failed: %d", i2cres);
            printToConsoleI2cError(i2cres);
        }
    }
    return -1;
}

SL_ConfigResult_t SaankhyaPHYAndroid::configPlatformParams() {

    SL_ConfigResult_t res;

#define SL_DEMOD_OUTPUT_SDIO 1
    /*
     * Assign Platform Configuration Parameters. For other ref platforms, replace settings from
     * comments above
     */
    sPlfConfig.chipType = SL_CHIP_3010;
    sPlfConfig.chipRev = SL_CHIP_REV_AA;
    sPlfConfig.boardType = SL_KAILASH_DONGLE;
    sPlfConfig.tunerType = TUNER_SI;
    sPlfConfig.demodControlIf = SL_DEMOD_CMD_CONTROL_IF_I2C;
    sPlfConfig.demodOutputIf = SL_DEMOD_OUTPUTIF_TS;
    sPlfConfig.demodI2cAddr = 0x30; /* SLDemod 7-bit Physical I2C Address */

#ifdef SL_FX3S
    sPlfConfig.demodResetGpioPin = 47;   /* FX3S GPIO 47 connected to Demod Reset Pin */
    sPlfConfig.cpldResetGpioPin = 43;   /* FX3S GPIO 43 connected to CPLD Reset Pin and used only for serial TS Interface  */
    sPlfConfig.demodI2cAddr3GpioPin = 37;   /* FX3S GPIO 37 connected to Demod I2C Address3 Pin and used only for SDIO Interface */
#endif

    /*
     * Relative Path to SLSDK from working directory
     * Example: D:\UNAME\PROJECTS\slsdk
     * User can just specifying "..", which will point to this directory or can specify full directory path explicitly
     */
    sPlfConfig.slsdkPath = ".";

    /* Set Configuration Parameters */
    res = SL_ConfigSetPlatform(sPlfConfig);

    printf("configPlatformParams: with boardType: %d", sPlfConfig.boardType);

    return res;

}



void SaankhyaPHYAndroid::handleCmdIfFailure(void)
{
    SL_Printf("\n SL CMD IF FAILURE: Cannot continue!");
    SL_DemodUnInit(slUnit);
    SL_TunerUnInit(tUnit);
    //processFlag = 0;
    //diagFlag = 0;
}

void SaankhyaPHYAndroid::printToConsoleI2cError(SL_I2cResult_t err)
{
    switch (err)
    {
        case SL_I2C_ERR_TRANSFER_FAILED:
            SL_Printf(" Sl I2C Transfer Failed");
            break;
        case SL_I2C_ERR_NOT_INITIALIZED:
            SL_Printf(" Sl I2C Not Initialized");
            break;

        case SL_I2C_ERR_BUS_TIMEOUT:
            SL_Printf(" Sl I2C Bus Timeout");
            break;

        case SL_I2C_ERR_LOST_ARBITRATION:
            SL_Printf(" Sl I2C Lost Arbitration");
            break;

        default:
            break;
    }
}

void SaankhyaPHYAndroid::printToConsoleTunerError(SL_TunerResult_t err)
{
    switch (err)
    {
        case SL_TUNER_ERR_OPERATION_FAILED:
            SL_Printf(" Sl Tuner Operation Failed");
            break;

        case SL_TUNER_ERR_INVALID_ARGS:
            SL_Printf(" Sl Tuner Invalid Argument");
            break;

        case SL_TUNER_ERR_NOT_SUPPORTED:
            SL_Printf(" Sl Tuner Not Supported");
            break;

        case SL_TUNER_ERR_MAX_INSTANCES_REACHED:
            SL_Printf(" Sl Tuner Maximum Instance Reached");
            break;
        default:
            break;
    }
}

void SaankhyaPHYAndroid::printToConsolePlfConfiguration(SL_PlatFormConfigParams_t cfgInfo)
{
    SL_Printf("\n\n SL Platform Configuration");
    switch (cfgInfo.boardType)
    {
        case SL_EVB_3000:
            SL_Printf("\n Board Type             : SL_EVB_3000");
            break;

        case SL_EVB_3010:
            SL_Printf("\n Board Type             : SL_EVB_3010");
            break;

        case SL_EVB_4000:
            SL_Printf("\n Board Type             : SL_EVB_4000");
            break;

        case SL_KAILASH_DONGLE:
            SL_Printf("\n Board Type             : SL_KAILASH_DONGLE");
            break;

        case SL_BORQS_EVT:
            SL_Printf("\n Board Type             : SL_BORQS_EVT");
            break;

        default:
            SL_Printf("\n Board Type             : NA");
    }

    switch (cfgInfo.chipType)
    {
        case SL_CHIP_3000:
            SL_Printf("\n Chip Type              : SL_CHIP_3000");
            break;

        case SL_CHIP_3010:
            SL_Printf("\n Chip Type              : SL_CHIP_3010");
            break;

        case SL_CHIP_4000:
            SL_Printf("\n Chip Type              : SL_CHIP_4000");
            break;

        default:
            SL_Printf("\n Chip Type              : NA");
    }

    if (cfgInfo.chipRev == SL_CHIP_REV_AA)
    {
        SL_Printf("\n Chip Revision          : SL_CHIP_REV_AA");
    }
    else if (cfgInfo.chipRev == SL_CHIP_REV_BB)
    {
        SL_Printf("\n Chip Revision          : SL_CHIP_REV_BB");
    }
    else
    {
        SL_Printf("\n Chip Revision          : NA");
    }

    if (cfgInfo.tunerType == TUNER_NXP)
    {
        SL_Printf("\n Tuner Type             : TUNER_NXP");
    }
    else if (cfgInfo.tunerType == TUNER_SI)
    {
        SL_Printf("\n Tuner Type             : TUNER_SI");
    }
    else
    {
        SL_Printf("\n Tuner Type             : NA");
    }

    switch (cfgInfo.demodControlIf)
    {
        case SL_DEMOD_CMD_CONTROL_IF_I2C:
            SL_Printf("\n Command Interface      : SL_DEMOD_CMD_CONTROL_IF_I2C");
            break;

        case SL_DEMOD_CMD_CONTROL_IF_SDIO:
            SL_Printf("\n Command Interface      : SL_DEMOD_CMD_CONTROL_IF_SDIO");
            break;

        case SL_DEMOD_CMD_CONTROL_IF_SPI:
            SL_Printf("\n Command Interface      : SL_DEMOD_CMD_CONTROL_IF_SPI");
            break;

        default:
            SL_Printf("\n Command Interface      : NA");
    }

    switch (cfgInfo.demodOutputIf)
    {
        case SL_DEMOD_OUTPUTIF_TS:
            SL_Printf("\n Output Interface       : SL_DEMOD_OUTPUTIF_TS");
            break;

        case SL_DEMOD_OUTPUTIF_SDIO:
            SL_Printf("\n Output Interface       : SL_DEMOD_OUTPUTIF_SDIO");
            break;

        case SL_DEMOD_OUTPUTIF_SPI:
            SL_Printf("\n Output Interface       : SL_DEMOD_OUTPUTIF_SPI");
            break;

        default:
            SL_Printf("\n Output Interface       : NA");
    }

    SL_Printf("\n Demod I2C Address      : 0x%x\n", cfgInfo.demodI2cAddr);
}

void SaankhyaPHYAndroid::printToConsoleDemodConfiguration(SL_DemodConfigInfo_t cfgInfo)
{
    SL_Printf("\n\n SL Demod Configuration");
    switch (cfgInfo.std)
    {
        case SL_DEMODSTD_ATSC3_0:
            SL_Printf("\n Standard               : ATSC3_0");
            break;

        case SL_DEMODSTD_ATSC1_0:
            SL_Printf("\n Demod Standard         : ATSC1_0");
            break;

        default:
            SL_Printf("\n Demod Standard         : NA");
    }

    SL_Printf("\n PLP Configuration");
    SL_Printf("\n   PLP0                 : %d", (signed char)cfgInfo.plpInfo.plp0);
    SL_Printf("\n   PLP1                 : %d", (signed char)cfgInfo.plpInfo.plp1);
    SL_Printf("\n   PLP2                 : %d", (signed char)cfgInfo.plpInfo.plp2);
    SL_Printf("\n   PLP3                 : %d", (signed char)cfgInfo.plpInfo.plp3);

    SL_Printf("\n Input Configuration");
    switch (cfgInfo.afeIfInfo.iftype)
    {
        case SL_IFTYPE_ZIF:
            SL_Printf("\n   IF Type              : ZIF");
            break;

        case SL_IFTYPE_LIF:
            SL_Printf("\n   IF Type              : LIF");
            break;

        default:
            SL_Printf("\n   IF Type              : NA");
    }

    switch (cfgInfo.afeIfInfo.iqswap)
    {
        case SL_IQSWAP_DISABLE:
            SL_Printf("\n   IQSWAP               : DISABLE");
            break;

        case SL_IQSWAP_ENABLE:
            SL_Printf("\n   IQSWAP               : ENABLE");
            break;

        default:
            SL_Printf("\n   IQSWAP               : NA");
    }

    switch (cfgInfo.afeIfInfo.iswap)
    {
        case SL_IPOL_SWAP_DISABLE:
            SL_Printf("\n   ISWAP                : DISABLE");
            break;

        case SL_IPOL_SWAP_ENABLE:
            SL_Printf("\n   ISWAP                : ENABLE");
            break;

        default:
            SL_Printf("\n   ISWAP                : NA");
    }

    switch (cfgInfo.afeIfInfo.qswap)
    {
        case SL_QPOL_SWAP_DISABLE:
            SL_Printf("\n   QSWAP                : DISABLE");
            break;

        case SL_QPOL_SWAP_ENABLE:
            SL_Printf("\n   QSWAP                : ENABLE");
            break;

        default:
            SL_Printf("\n   QSWAP                : NA");
    }

    SL_Printf("\n   ICoeff1              : %3.4f", cfgInfo.iqOffCorInfo.iCoeff1);
    SL_Printf("\n   QCoeff1              : %3.4f", cfgInfo.iqOffCorInfo.qCoeff1);
    SL_Printf("\n   ICoeff2              : %3.4f", cfgInfo.iqOffCorInfo.iCoeff2);
    SL_Printf("\n   QCoeff2              : %3.4f", cfgInfo.iqOffCorInfo.qCoeff2);

    SL_Printf("\n   AGCReference         : %d mv", cfgInfo.afeIfInfo.agcRefValue);
    SL_Printf("\n   Tuner IF Frequency   : %3.2f MHz", cfgInfo.afeIfInfo.ifreq);

    SL_Printf("\n Output Configuration");
    switch (cfgInfo.oifInfo.oif)
    {
        case SL_OUTPUTIF_TSPARALLEL_LSB_FIRST:
            SL_Printf("\n   OutputInteface       : TS PARALLEL LSB FIRST");
            break;

        case SL_OUTPUTIF_TSPARALLEL_MSB_FIRST:
            SL_Printf("\n   OutputInteface       : TS PARALLEL MSB FIRST");
            break;

        case SL_OUTPUTIF_TSSERIAL_LSB_FIRST:
            SL_Printf("\n   OutputInteface       : TS SERAIL LSB FIRST");
            break;

        case SL_OUTPUTIF_TSSERIAL_MSB_FIRST:
            SL_Printf("\n   OutputInteface       : TS SERIAL MSB FIRST");
            break;

        case SL_OUTPUTIF_SDIO:
            SL_Printf("\n   OutputInteface       : SDIO");
            break;

        case SL_OUTPUTIF_SPI:
            SL_Printf("\n   OutputInteface       : SPI");
            break;

        default:
            SL_Printf("\n   OutputInteface       : NA");
    }

    switch (cfgInfo.oifInfo.TsoClockInvEnable)
    {
        case SL_TSO_CLK_INV_OFF:
            SL_Printf("\n   TS Out Clock Inv     : DISABLED");
            break;

        case SL_TSO_CLK_INV_ON:
            SL_Printf("\n   TS Out Clock Inv     : ENABLED");
            break;

        default:
            SL_Printf("\n    TS Out Clock Inv    : NA");
    }
}

void SaankhyaPHYAndroid::printToConsoleDemodError(SL_Result_t err)
{
    switch (err)
    {
        case SL_ERR_OPERATION_FAILED:
            SL_Printf(" Sl Operation Failed");
            break;

        case SL_ERR_TOO_MANY_INSTANCES:
            SL_Printf(" Sl Too Many Instance");
            break;

        case SL_ERR_CODE_DWNLD:
            SL_Printf(" Sl Code download Failed");
            break;

        case SL_ERR_INVALID_ARGUMENTS:
            SL_Printf(" Sl Invalid Argument");
            break;

        case SL_ERR_CMD_IF_FAILURE:
            SL_Printf(" Sl Command Interface Failure");
            break;

        case SL_ERR_NOT_SUPPORTED:
            SL_Printf(" Sl Not Supported");
            break;
        default:
            break;
    }
}




int SaankhyaPHYAndroid::pinFromRxCaptureThread() {
    printf("atsc3NdkClient::Atsc3_Jni_Processing_Thread_Env: mJavaVM: %p", atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    Atsc3_Jni_Capture_Thread_Env = new Atsc3JniEnv(atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    return 0;
};

int SaankhyaPHYAndroid::pinFromRxProcessingThread() {
    printf("atsc3NdkClient::pinFromRxProcessingThread: mJavaVM: %p", atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    Atsc3_Jni_Processing_Thread_Env = new Atsc3JniEnv(atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    return 0;
}


int SaankhyaPHYAndroid::pinFromRxStatusThread() {
    printf("atsc3NdkClient::pinFromRxStatusThread: mJavaVM: %p", atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    Atsc3_Jni_Status_Thread_Env = new Atsc3JniEnv(atsc3_ndk_phy_saankhya_static_loader_get_javaVM());
    return 0;
}


void* SaankhyaPHYAndroid::ProcessThread(void* context)
//#endif
{
    printf("atsc3NdkClientSlImpl::ProcessThread: with context: %p", context);

    SaankhyaPHYAndroid* apiImpl = (SaankhyaPHYAndroid*) context;

    apiImpl->resetProcessThreadStatistics();

    //TODO: wire this up to our atsvc3NdkClientSL::Atsc3_Jni_Processing_Thread_Env
    (SaankhyaPHYAndroid*)apiImpl->pinFromRxProcessingThread();

    while (apiImpl->processThreadShouldRun)
    {
        //printf("atsc3NdkClientSlImpl::ProcessThread: getDataSize is: %d", CircularBufferGetDataSize(cb));

        //hack
        while(CircularBufferGetDataSize(apiImpl->cb) >= BUFFER_SIZE) {
            apiImpl->processTLVFromCallback();
        }
        usleep(10000);
    }

    return 0;
}


//#elif __ANDROID__
void* SaankhyaPHYAndroid::CaptureThread(void* context)
//#endif
{
    SaankhyaPHYAndroid* apiImpl = (SaankhyaPHYAndroid*) context;

    (SaankhyaPHYAndroid*)apiImpl->pinFromRxCaptureThread();

    SL_RxDataStart((RxDataCB)&SaankhyaPHYAndroid::RxDataCallback);
    return 0;
}

//#ifdef _WIN32
//DWORD WINAPI TunerStatusThread(LPVOID lpParam)
//#elif linux
//int TunerStatusThread()
//#elif __ANDROID__
void* SaankhyaPHYAndroid::TunerStatusThread(void* context)
{

    SaankhyaPHYAndroid* apiImpl = (SaankhyaPHYAndroid*) context;

    //TODO: wire this up to our atsvc3NdkClientSL::pinFromRxStatusThread
    (SaankhyaPHYAndroid*)apiImpl->pinFromRxStatusThread();

    SL_TunerResult_t tres;
    SL_TunerSignalInfo_t tunerInfo;
    SL_DemodLockStatus_t demodLockStatus;
    uint cpuStatus = 0;
    uint lastCpuStatus = 0;
    SL_Result_t dres;

    unsigned long long llsPlpInfo;

    SL_Atsc3p0Perf_Diag_t perfDiag;
    SL_Atsc3p0Bsr_Diag_t  bsrDiag;
    SL_Atsc3p0L1B_Diag_t  l1bDiag;
    SL_Atsc3p0L1D_Diag_t  l1dDiag;

    double snr;
    double ber_l1b;
    double ber_l1d;
    double ber_plp0;

    //atsc3NdkClientSlImpl::tunerStatusThreadShouldRun
    while(true) {

        //only actively poll the tuner status if the RF status window is visible
//        if(!atsc3NdkClientSlImpl::tunerStatusThreadShouldPollTunerStatus) {
//            usleep(1000000);
//            continue;
//        }

        if(lastCpuStatus == 0xFFFFFFFF) {
            usleep(1000000); //jjustman: target: sleep for 500ms
            //TODO: jjustman-2019-12-05: investigate FX3 firmware and i2c single threaded interrupt handling instead of dma xfer
        } else {
            usleep(2500000);
        }
        lastCpuStatus = 0;

        /*jjustman-2020-01-06: For the SL3000/SiTune, we will have 3 status attributes with the following possible values:

                tunerInfo.status:   SL_TUNER_STATUS_NOT_LOCKED (0)
                                    SL_TUNER_STATUS_LOCKED (1)

                demodLockStatus:    SL_DEMOD_STATUS_NOT_LOCK (0)
                                    SL_DEMOD_STATUS_LOCK (1)

                cpuStatus:          (cpuStatus == 0xFFFFFFFF) ? "RUNNING" : "HALTED",
         */


        tres = SL_TunerGetStatus(apiImpl->tUnit, &tunerInfo);
        if (tres != SL_TUNER_OK) {
            //atsc3NdkClientSlImpl::atsc3NdkClientSLRef->LogMsgF("Error:SL_TunerGetStatus: deviceHandle: %p, res: %d", __deviceHandle_FIXME, tres);
            printf("\n Error:SL_TunerGetStatus: tres: %d", tres);
            //printToConsoleTunerError(tres);
            continue;
        }

        dres = SL_DemodGetStatus(apiImpl->slUnit, SL_DEMOD_STATUS_TYPE_LOCK, (SL_DemodLockStatus_t*)&demodLockStatus);
        if (dres != SL_OK) {
            printf("\n Error:SL_Demod Get Lock Status                :");
            // printToConsoleDemodError(dres);
            continue;
        }

        dres = SL_DemodGetStatus(apiImpl->slUnit, SL_DEMOD_STATUS_TYPE_CPU, (int*)&cpuStatus);
        if (dres != SL_OK) {
            printf("\n Error:SL_Demod Get CPU Status                 :");
            //printToConsoleDemodError(dres);
            continue;
        }

        lastCpuStatus = cpuStatus;

        dres = SL_DemodGetAtsc3p0Diagnostics(apiImpl->slUnit, SL_DEMOD_DIAG_TYPE_PERF, (SL_Atsc3p0Perf_Diag_t*)&perfDiag);
        if (dres != SL_OK) {
            printf("\n Error getting ATSC3.0 Performance Diagnostics :");
            // printToConsoleDemodError(dres);
            continue;
        }

        dres = SL_DemodGetAtsc3p0Diagnostics(apiImpl->slUnit, SL_DEMOD_DIAG_TYPE_BSR, (SL_Atsc3p0Bsr_Diag_t*)&bsrDiag);
        if (dres != SL_OK) {
            printf("\n Error getting ATSC3.0 Boot Strap Diagnostics  :");
            //  printToConsoleDemodError(dres);
            continue;
        }

        dres = SL_DemodGetAtsc3p0Diagnostics(apiImpl->slUnit, SL_DEMOD_DIAG_TYPE_L1B, (SL_Atsc3p0L1B_Diag_t*)&l1bDiag);
        if (dres != SL_OK) {
            printf("\n Error getting ATSC3.0 L1B Diagnostics         :");
            // printToConsoleDemodError(dres);
            continue;
        }

        dres = SL_DemodGetAtsc3p0Diagnostics(apiImpl->slUnit, SL_DEMOD_DIAG_TYPE_L1D, (SL_Atsc3p0L1D_Diag_t*)&l1dDiag);
        if (dres != SL_OK) {
            printf("\n Error getting ATSC3.0 L1D Diagnostics         :");
            //    printToConsoleDemodError(dres);
            continue;
        }

        int slres = SL_DemodGetLlsPlpList(apiImpl->slUnit, &llsPlpInfo);
        if (slres != SL_OK) {
            printf("\n Error:SL_DemodGetLlsPlpList :");
            continue;
        }


        snr = (float)perfDiag.GlobalSnrLinearScale / 16384;
        snr = 10000.0 * log10(snr); //10

        ber_l1b = (float)perfDiag.NumBitErrL1b / perfDiag.NumFecBitsL1b; // //aBerPreLdpcE7,
        ber_l1d = (float)perfDiag.NumBitErrL1d / perfDiag.NumFecBitsL1d;//aBerPreBchE9,
        ber_plp0 = (float)perfDiag.NumBitErrPlp0 / perfDiag.NumFecBitsPlp0; //aFerPostBchE6,

        printf("atsc3NdkClientSlImpl::TunerStatusThread: tunerInfo.status: %d, tunerInfo.signalStrength: %f, cpuStatus: %s, demodLockStatus: %d, globalSnr: %f",
               tunerInfo.status,
               tunerInfo.signalStrength,
               (cpuStatus == 0xFFFFFFFF) ? "RUNNING" : "HALTED",
               demodLockStatus,
               perfDiag.GlobalSnrLinearScale);


//        apiImpl->atsc3_update_rf_stats(tunerInfo.status == 1,
//                tunerInfo.signalStrength,
//                apiImpl.plpInfo.plp0 == l1dDiag.sf0ParamsStr.Plp0paramsstr.L1dSfPlpId,
//                l1dDiag.sf0ParamsStr.Plp0paramsstr.L1dSfPlpFecType,
//                l1dDiag.sf0ParamsStr.Plp0paramsstr.L1dSfPlpModType,
//                l1dDiag.sf0ParamsStr.Plp0paramsstr.L1dSfPlpCoderate,
//                tunerInfo.signalStrength/1000,
//                snr,
//                ber_l1b,
//                ber_l1d,
//                ber_plp0,
//                demodLockStatus,
//                cpuStatus == 0xFFFFFFFF,
//                llsPlpInfo & 0x01 == 0x01,
//                0);
//
//        atsc3NdkClientSLRef->atsc3_update_rf_bw_stats(apiImpl.alp_completed_packets_parsed, apiImpl.alp_total_bytes, apiImpl.alp_total_LMTs_recv);

    }
    return 0;
}

void SaankhyaPHYAndroid::processTLVFromCallback()
{
    int bytesRead = CircularBufferPop(cb, BUFFER_SIZE, (char*)&processDataCircularBufferForCallback);
    if(!atsc3_sl_tlv_block) {
        printf("ERROR: atsc3NdkClientSlImpl::processTLVFromCallback - atsc3_sl_tlv_block is NULL!");
        return;
    }

    if(bytesRead) {

        block_Write(atsc3_sl_tlv_block, (uint8_t*)&processDataCircularBufferForCallback, bytesRead);
        block_Rewind(atsc3_sl_tlv_block);

        bool atsc3_sl_tlv_payload_complete = false;

        do {
            atsc3_sl_tlv_payload = atsc3_sl_tlv_payload_parse_from_block_t(atsc3_sl_tlv_block);

            if(atsc3_sl_tlv_payload) {
                atsc3_sl_tlv_payload_dump(atsc3_sl_tlv_payload);
                if(atsc3_sl_tlv_payload->alp_payload_complete) {
                    atsc3_sl_tlv_payload_complete = true;

                    block_Rewind(atsc3_sl_tlv_payload->alp_payload);
                    atsc3_alp_packet_t* atsc3_alp_packet = atsc3_alp_packet_parse(atsc3_sl_tlv_payload->plp_number, atsc3_sl_tlv_payload->alp_payload);
                    if(atsc3_alp_packet) {
                        alp_completed_packets_parsed++;

                        alp_total_bytes += atsc3_alp_packet->alp_payload->p_size;

                        if(atsc3_alp_packet->alp_packet_header.packet_type == 0x00) {

                            block_Rewind(atsc3_alp_packet->alp_payload);
                            //atsc3_phy_mmt_player_bridge_process_packet_phy(atsc3_alp_packet->alp_payload);
                            atsc3_core_service_bridge_process_packet_phy(atsc3_alp_packet->alp_payload);
                        } else if(atsc3_alp_packet->alp_packet_header.packet_type == 0x4) {
                            alp_total_LMTs_recv++;
                        }

                        atsc3_alp_packet_free(&atsc3_alp_packet);
                    }
                    //free our atsc3_sl_tlv_payload
                    atsc3_sl_tlv_payload_free(&atsc3_sl_tlv_payload);

                } else {
                    atsc3_sl_tlv_payload_complete = false;
                    //jjustman-2019-12-29 - noisy, TODO: wrap in __DEBUG macro check
                    //printf("alp_payload->alp_payload_complete == FALSE at pos: %d, size: %d", atsc3_sl_tlv_block->i_pos, atsc3_sl_tlv_block->p_size);
                }
            } else {
                atsc3_sl_tlv_payload_complete = false;
                //jjustman-2019-12-29 - noisy, TODO: wrap in __DEBUG macro check
                //printf("ERROR: alp_payload IS NULL, short TLV read?  at atsc3_sl_tlv_block: i_pos: %d, p_size: %d", atsc3_sl_tlv_block->i_pos, atsc3_sl_tlv_block->p_size);
            }

        } while(atsc3_sl_tlv_payload_complete);


        if(atsc3_sl_tlv_payload && !atsc3_sl_tlv_payload->alp_payload_complete && atsc3_sl_tlv_block->i_pos > atsc3_sl_tlv_payload->sl_tlv_total_parsed_payload_size) {
            //accumulate from our last starting point and continue iterating during next bbp
            atsc3_sl_tlv_block->i_pos -= atsc3_sl_tlv_payload->sl_tlv_total_parsed_payload_size;
            //free our atsc3_sl_tlv_payload
            atsc3_sl_tlv_payload_free(&atsc3_sl_tlv_payload);
        }

        if(atsc3_sl_tlv_block->p_size > atsc3_sl_tlv_block->i_pos) {
            //truncate our current block_t starting at i_pos, and then read next i/o block
            block_t* temp_sl_tlv_block = block_Duplicate_from_position(atsc3_sl_tlv_block);
            block_Destroy(&atsc3_sl_tlv_block);
            atsc3_sl_tlv_block = temp_sl_tlv_block;
            block_Seek(atsc3_sl_tlv_block, atsc3_sl_tlv_block->p_size);
        } else if(atsc3_sl_tlv_block->p_size == atsc3_sl_tlv_block->i_pos) {
            //clear out our tlv block as we are the "exact" size of our last alp packet

            block_Destroy(&atsc3_sl_tlv_block);
            atsc3_sl_tlv_block = block_Alloc(BUFFER_SIZE);
        } else {
            printf("atsc3_sl_tlv_block: positioning mismatch: i_pos: %d, p_size: %d - rewinding and seeking for magic packet?", atsc3_sl_tlv_block->i_pos, atsc3_sl_tlv_block->p_size);

            //jjustman: 2019-11-23: rewind in order to try seek for our magic packet in the next loop here
            block_Rewind(atsc3_sl_tlv_block);
        }
    }
}

void SaankhyaPHYAndroid::RxDataCallback(unsigned char *data, long len)
{
    //printf("atsc3NdkClientSlImpl::RxDataCallback: pushing data: %p, len: %d", data, len);
    CircularBufferPush(SaankhyaPHYAndroid::cb, (char *)data, len);
}


extern "C"
JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_init(JNIEnv *env, jobject instance) {
    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_init: start init, env: %p", env);
    saankhyaPHYAndroid = new SaankhyaPHYAndroid(env, instance);
    saankhyaPHYAndroid->init();

    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_init: return init, env: %p", env);
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_run(JNIEnv *env, jobject thiz) {
    int res = 0;
    if(!saankhyaPHYAndroid) {
        _SAANKHYA_PHY_ANDROID_ERROR("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_run: error, srtRxSTLTPVirtualPHYAndroid is NULL!");
        return -1;
    }
    res = saankhyaPHYAndroid->run();
    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_run: returning res: %d", res);

    return res;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_is_1running(JNIEnv* env, jobject instance)
{
    jboolean res = false;

    if(!saankhyaPHYAndroid) {
        _SAANKHYA_PHY_ANDROID_ERROR("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_is_1running: error, srtRxSTLTPVirtualPHYAndroid is NULL!");
        return false;
    }
    res = saankhyaPHYAndroid->is_running();
    return res;
}


extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_stop(JNIEnv *env, jobject thiz) {
    int res = 0;
    if(!saankhyaPHYAndroid) {
        _SAANKHYA_PHY_ANDROID_ERROR("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_stop: error, srtRxSTLTPVirtualPHYAndroid is NULL!");
        return -1;
    }
    res = saankhyaPHYAndroid->stop();
    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_stop: returning res: %d", res);

    return res;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_deinit(JNIEnv *env, jobject thiz) {
    int res = 0;
    if(!saankhyaPHYAndroid) {
        _SAANKHYA_PHY_ANDROID_ERROR("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_deinit: error, srtRxSTLTPVirtualPHYAndroid is NULL!");
        return -1;
    }

    saankhyaPHYAndroid->deinit();
    saankhyaPHYAndroid = nullptr;

    return res;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_download_1bootloader_1firmware(JNIEnv *env, jobject thiz, jint fd) {
    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_download_1bootloader_1firmware: fd: %d", fd);
    int res = 0;

    res = saankhyaPHYAndroid->download_bootloader_firmware(fd); //calls pre_init

    return res;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_open(JNIEnv *env, jobject thiz, jint fd) {
    _SAANKHYA_PHY_ANDROID_DEBUG("Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_open: fd: %d", fd);

    int res = 0;
    res = saankhyaPHYAndroid->open(fd, 0, 0);

    return res;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_tune(JNIEnv *env, jobject thiz,
                                                                      jint freq_khz,
                                                                      jint single_plp) {
    int res = 0;

    res = saankhyaPHYAndroid->tune(freq_khz, single_plp);

    return res;
}
extern "C" JNIEXPORT jint JNICALL
Java_org_ngbp_libatsc3_middleware_android_phy_SaankhyaPHYAndroid_listen_1plps(JNIEnv *env,
                                                                              jobject thiz,
                                                                              jobject plps) {
    int res = 0;

    vector<uint8_t> listen_plps;

    jobject jIterator = env->CallObjectMethod(plps, env->GetMethodID(env->GetObjectClass(plps), "iterator", "()Ljava/util/Iterator;"));
    jmethodID nextMid = env->GetMethodID(env->GetObjectClass(jIterator), "next", "()Ljava/lang/Object;");
    jmethodID hasNextMid = env->GetMethodID(env->GetObjectClass(jIterator), "hasNext", "()Z");

    while (env->CallBooleanMethod(jIterator, hasNextMid)) {
        jobject jItem = env->CallObjectMethod(jIterator, nextMid);
        jbyte jByte = env->CallByteMethod(jItem, env->GetMethodID(env->GetObjectClass(jItem), "byteValue", "()B"));
        listen_plps.push_back(jByte);
    }

    res = saankhyaPHYAndroid->listen_plps(listen_plps);

    return res;
    }