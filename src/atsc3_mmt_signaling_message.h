/*
 * atsc3_mmt_signaling_message.h
 *
 *  Created on: Jan 21, 2019
 *      Author: jjustman
 */


/**
 *
 * Borrowed from A/331 Section 7.2.3
 * ATSC A/331:2017 Signaling, Delivery, Synchronization, and Error Protection 6 December 2017
 *
 * MMTP-Specific Signaling Message
 *
 * When MMTP sessions are used to carry an ATSC 3.0 streaming service, MMTP-specific signaling messages specified in Clause 10
 * of ISO/IEC 23008-1 [37] are delivered in binary format by MMTP packets according to Signaling Message Mode specified in
 * subclause 9.3.4 of ISO/IEC 23008-1 [37].
 *
 * The value of the packet_id field of MMTP packets carrying Service Layer Signaling shall be set to 0x0000 except for
 * MMTP packets carrying MMTP-specific signaling messages specific to an Asset, which shall
 * be set to 0x0000 or the same packet_id value as the MMTP packets carrying the Asset.
 *
 * Identifiers referencing the appropriate Package for each ATSC 3.0 Service are signaled by the USBD fragment as
 * described in Table 7.4.
 *
 * MMT Package Table (MPT) messages with matching MMT_package_id shall be delivered on the MMTP session signaled in the SLT.
 *
 * Each MMTP session carries MMTP-specific signaling messages specific to its session or each asset delivered by the MMTP session.
 *
 * The following MMTP messages shall be delivered by the MMTP session signaled in the SLT:
 *
 *    MMT Package Table (MPT) message: This message carries an MP (MMT Package) table which contains the list of all Assets and
 * their location information as specified in subclause 10.3.4 of ISO/IEC 23008-1) [37].
 *    MMT ATSC3 (MA3) message mmt_atsc3_message(): This message carries system metadata specific for ATSC 3.0 services including
 *
 * Service Layer Signaling as specified in Section 7.2.3.1.
 *
 * The following MMTP messages shall be delivered by the MMTP session signaled in the SLT, if required:
 *
 *   Media Presentation Information (MPI) message: This message carries an MPI table which contains the whole document or a
 * subset of a document of presentation information. An MP table associated with the MPI table also can be delivered by this
 * message (see subclause 10.3.3 of ISO/IEC 23008-1) [37];
 *
 * The following MMTP messages shall be delivered by the MMTP session carrying an associated Asset and the value of the
 * packet_id field of MMTP packets carrying them shall be set to the same as the MMTP packets carrying the Asset::
 *
 *   Hypothetical Receiver Buffer Model message: This message carries information required by the receiver to manage its
 * buffer (see subclause 10.4.2 of ISO/IEC 23008-1 [37]);
 *
 *   Hypothetical Receiver Buffer Model Removal message: This message carries information required by the receiver to
 * manage its MMT de-capsulation buffer (see subclause 10.4.9 of ISO/IEC 23008-1) [37];
 *
 */


#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "atsc3_utils.h"
#include "atsc3_mmtp_types.h"
#include "atsc3_gzip.h"

#ifndef MODULES_DEMUX_MMT_ATSC3_MMT_SIGNALING_MESSAGE_H_
#define MODULES_DEMUX_MMT_ATSC3_MMT_SIGNALING_MESSAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int _MMT_SIGNALLING_MESSAGE_DEBUG_ENABLED;
extern int _MMT_SIGNALLING_MESSAGE_TRACE_ENABLED;

/**
 *
 * MPU_timestamp_descriptor message example
 *
0000   62 02 00 23 af b9 00 00 00 2b 4f 2f 00 35 10 58   b..#¯¹...+O/.5.X
0010   a4 00 00 00 00 12 ce 00 3f 12 ce 00 3b 04 01 00   ¤.....Î.?.Î.;...
0020   00 00 00 00 00 00 00 10 11 11 11 11 11 11 11 11   ................
0030   11 11 11 11 11 11 11 11 68 65 76 31 fd 00 ff 00   ........hev1ý.ÿ.
0040   01 5f 90 01 00 00 23 00 0f 00 01 0c 00 00 16 ce   ._....#........Î
0050   df c2 af b8 d6 45 9f ff                           ßÂ¯¸ÖE.ÿ

raw base64 payload:

62020023afb90000002b4f2f00351058a40000000012ce003f12ce003b04010000000000000000101111111111111111111111111111111168657631fd00ff00015f9001000023000f00010c000016cedfc2afb8d6459fff
 *
 */

mmt_signalling_message_vector_t* mmt_signalling_message_vector_create();
mmt_signalling_message_vector_t* mmt_signalling_message_vector_add(mmt_signalling_message_vector_t* mmt_signalling_message_vector, mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload);
mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload_create(uint16_t message_id, uint8_t version);

void mmt_signalling_message_vector_free(mmt_signalling_message_vector_t**);

uint8_t* mmt_signaling_message_parse_packet_header(mmtp_payload_fragments_union_t* si_message, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mmt_signaling_message_parse_packet(mmtp_payload_fragments_union_t *si_message, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mmt_signaling_message_parse_id_type(mmtp_payload_fragments_union_t *si_message, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
void mmt_signalling_message_header_and_payload_free(mmt_signalling_message_header_and_payload_t**);

uint8_t* pa_message_parse(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mpi_message_parse(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mpt_message_parse(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mmt_atsc3_message_payload_parse(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
uint8_t* mmt_scte35_message_payload_parse(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);
ATSC3_VECTOR_BUILDER_METHODS_INTERFACE(mmt_scte35_message_payload, mmt_scte35_signal_descriptor)


uint8_t* si_message_not_supported(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload, uint8_t* udp_raw_buf, uint32_t udp_raw_buf_size);



void signaling_message_dump(mmtp_payload_fragments_union_t* si_message);
void pa_message_dump(mmtp_payload_fragments_union_t* mmtp_payload_fragments);
void mpi_message_dump(mmtp_payload_fragments_union_t* mmtp_payload_fragments);
void mpt_message_dump(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload_t);
void mmt_atsc3_message_payload_dump(mmt_signalling_message_header_and_payload_t* mmt_signalling_message_header_and_payload);




#define _MMSM_PRINTLN(...) printf(__VA_ARGS__);printf("%s%s","\r","\n")
#define _MMSM_ERROR(...)   printf("%s:%d:ERROR: ",__FILE__,__LINE__);_MMSM_PRINTLN(__VA_ARGS__);
#define _MMSM_WARN(...)    printf("%s:%d:WARN: ",__FILE__,__LINE__);_MMSM_PRINTLN(__VA_ARGS__);
#define _MMSM_INFO(...)    printf("%s:%d:INFO: ",__FILE__,__LINE__);_MMSM_PRINTLN(__VA_ARGS__);
#define _MMSM_DEBUG(...)   if(_MMT_SIGNALLING_MESSAGE_DEBUG_ENABLED) { printf("%s:%d:DEBUG: ",__FILE__,__LINE__);_MMSM_PRINTLN(__VA_ARGS__); }
#define _MMSM_TRACE(...)   if(_MMT_SIGNALLING_MESSAGE_TRACE_ENABLED) { printf("%s:%d:TRACE: ",__FILE__,__LINE__);_MMSM_PRINTLN(__VA_ARGS__); }

#ifdef __cplusplus
}
#endif

#endif /* MODULES_DEMUX_MMT_ATSC3_MMT_SIGNALING_MESSAGE_H_ */
