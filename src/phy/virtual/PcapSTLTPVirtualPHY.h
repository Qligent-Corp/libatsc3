//
// Created by Jason Justman on 2019-09-27.
//

#ifndef LIBATSC3_PCAPSTLTPVIRTUALPHY_H
#define LIBATSC3_PCAPSTLTPVIRTUALPHY_H

#include <string>
#include <thread>
#include <map>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <list>

#include <sys/types.h>

#include <string.h>
#include <stdlib.h>


using namespace std;

#define DEBUG 1

#include "../IAtsc3NdkPHYClient.h"

// libatsc3 type imports here
#include <atsc3_utils.h>
#include <atsc3_pcap_type.h>
#include <atsc3_stltp_parser.h>
#include <atsc3_alp_parser.h>
#include <atsc3_stltp_depacketizer.h>

/*
 * : public libatsc3_Iphy_mockable
 *  * defer: #include <atsc3_core_service_player_bridge.h>
 *   void atsc3_core_service_bridge_process_packet_phy(block_t* packet);
 *
 */

class PcapSTLTPVirtualPHY : public IAtsc3NdkPHYClient
{
public:
    PcapSTLTPVirtualPHY();

    int Init();
    int Prepare(const char *strDevListInfo, int delim1, int delim2);
    int Open(int fd, int bus, int addr);
    int Tune(int freqKHz, int plpId);
    int Stop();
    int Close();
    int Reset();
    int Uninit();

    /*
     * pcap methods
     */

    //configure one-shot listener for single PLP flow from STLTP
    void atsc3_pcap_stltp_listen_ip_port_plp(string ip, string port, uint8_t plp);

    int atsc3_pcap_replay_open_file(const char *filename);

    int atsc3_pcap_thread_run();
    int atsc3_pcap_thread_stop(); 							//will invoke cleanup of context

    bool is_pcap_replay_running();
    atsc3_pcap_replay_context_t* get_pcap_replay_context_status_volatile(); //treat this as const*

    //special "friend" callback from stltp_depacketizer context
    static void Atsc3_stltp_baseband_alp_packet_collection_callback_with_context(atsc3_alp_packet_collection_t* atsc3_alp_packet_collection, void* context);
    void atsc3_stltp_baseband_alp_packet_collection_received(atsc3_alp_packet_collection_t* atsc3_alp_packet_collection);

    ~PcapSTLTPVirtualPHY() {
    	atsc3_pcap_thread_stop(); //cleanup just to be sure..
    	atsc3_stltp_depacketizer_context_free(&atsc3_stltp_depacketizer_context);
    }
protected:

    //pcap replay context and locals
    int PcapProducerThreadParserRun();
    int PcapConsumerThreadRun();
    int PcapLocalCleanup();

    //overloadable callbacks for Android to pin mJavaVM as needed
    virtual void pinPcapProducerThreadAsNeeded() { };
    virtual void releasePinPcapProducerThreadAsNeeded() { };

    virtual void pinPcapConsumerThreadAsNeeded() { };
    virtual void releasePcapConsumerThreadAsNeeded() { };


    //local member variables for pcap replay

    char*                           pcap_replay_filename = NULL;
    bool                            pcapThreadShouldRun = false;

    std::thread                     pcapProducerThreadPtr;
    bool                            pcapProducerShutdown = true;

    std::thread                     pcapConsumerThreadPtr;
    bool                            pcapConsumerShutdown = true;

    atsc3_pcap_replay_context_t*    atsc3_pcap_replay_context = NULL;

    queue<block_t*>                 pcap_replay_buffer_queue;
    mutex                           pcap_replay_buffer_queue_mutex;
    condition_variable              pcap_replay_condition;

    //STLTP depacketizer context
    //build map of PLP to context's

    atsc3_stltp_depacketizer_context_t* 	atsc3_stltp_depacketizer_context;
    /*
     * depacketizer will need to dispatch via
     *             if(this->atsc3_rx_udp_packet_process_callback) {
            	this->atsc3_rx_udp_packet_process_callback(phy_payload_to_process);
            }
     *
     */



};

#define PCAP_DEMUXED_VIRTUAL_PHY_ERROR(...)   	__LIBATSC3_TIMESTAMP_ERROR(__VA_ARGS__);
#define PCAP_DEMUXED_VIRTUAL_PHY_WARN(...)   	__LIBATSC3_TIMESTAMP_WARN(__VA_ARGS__);
#define PCAP_DEMUXED_VIRTUAL_PHY_INFO(...)   	__LIBATSC3_TIMESTAMP_INFO(__VA_ARGS__);
#define PCAP_DEMUXED_VIRTUAL_PHY_DEBUG(...)   	__LIBATSC3_TIMESTAMP_DEBUG(__VA_ARGS__);



#endif //LIBATSC3_PCAPSTLTPVIRTUALPHY_H
