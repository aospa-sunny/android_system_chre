/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHPP_TRANSPORT_H_
#define CHPP_TRANSPORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chpp/link.h"
#include "chpp/macros.h"
#include "chpp/mutex.h"
#include "chpp/notifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************************************
 *  Public Definitions
 ***********************************************/

/**
 * CHPP Transport header flags bitmap
 *
 * @defgroup CHPP_TRANSPORT_FLAG
 * @{
 */
// This packet concludes a (fragmented or unfragmented) datagram
#define CHPP_TRANSPORT_FLAG_FINISHED_DATAGRAM 0x00
// Set if packet is part of a fragmented datagram, except for the last fragment
#define CHPP_TRANSPORT_FLAG_UNFINISHED_DATAGRAM 0x01
// Set for first packet after bootup or to reset after irrecoverable error
#define CHPP_TRANSPORT_FLAG_RESET 0x02
// Reserved for future use
#define CHPP_TRANSPORT_FLAG_RESERVED 0xfc
/** @} */

/**
 * Preamble (i.e. packet start delimiter).
 * Any future backwards-incompatible versions of CHPP Transport will use a
 * different preamble.
 *
 * @defgroup CHPP_PREAMBLE
 * @{
 */
#define CHPP_PREAMBLE_DATA 0x6843
#define CHPP_PREAMBLE_LEN_BYTES 2
/** @} */

/**
 * Macros for a specific byte in the CHPP_PREAMBLE.
 * Using the CHPP_PREAMBLE_BYTE_... macros are preferred due to a reduced risk
 * of mistakes.
 */
#define chppPreambleByte(loc) \
  ((CHPP_PREAMBLE_DATA >> (8 * (CHPP_PREAMBLE_LEN_BYTES - (loc)-1))) & 0xff)
#define CHPP_PREAMBLE_BYTE_FIRST chppPreambleByte(0)
#define CHPP_PREAMBLE_BYTE_SECOND chppPreambleByte(1)

/**
 * Maximum number of datagrams in the Tx queue.
 * CHPP will return an error if it is provided with a new Tx datagram when this
 * queue is full.
 * To be safe, this should be less than half of the maximum uint8_t value.
 * Otherwise, ChppTxDatagramQueue should be updated accordingly.
 */
#define CHPP_TX_DATAGRAM_QUEUE_LEN 16

/**
 * Maximum payload of packets at the link layer.
 * TODO: Negotiate or advertise MTU
 */
#define CHPP_LINK_TX_MTU_BYTES                                               \
  MAX(CHPP_PLATFORM_LINK_TX_MTU_BYTES,                                       \
      (1024 + CHPP_PREAMBLE_LEN_BYTES + sizeof(struct ChppTransportHeader) + \
       sizeof(struct ChppTransportFooter)))

/**
 * Maximum payload of packets at the transport layer.
 */
#define CHPP_TRANSPORT_TX_MTU_BYTES                   \
  (CHPP_LINK_TX_MTU_BYTES - CHPP_PREAMBLE_LEN_BYTES - \
   sizeof(struct ChppTransportHeader) - sizeof(struct ChppTransportFooter))

/************************************************
 *  Status variables to store context in lieu of global variables (this)
 ***********************************************/

/**
 * Error codes optionally reported in ChppTransportHeader (Least significant
 * nibble of int8_t packetCode).
 */
enum ChppTransportErrorCode {
  //! No error reported (either ACK or implicit NACK)
  CHPP_TRANSPORT_ERROR_NONE = 0,
  //! Checksum failure
  CHPP_TRANSPORT_ERROR_CHECKSUM = 1,
  //! Out of memory
  CHPP_TRANSPORT_ERROR_OOM = 2,
  //! Busy
  CHPP_TRANSPORT_ERROR_BUSY = 3,
  //! Invalid header
  CHPP_TRANSPORT_ERROR_HEADER = 4,
  //! Out of order
  CHPP_TRANSPORT_ERROR_ORDER = 5,
  //! Message incomprehensible at App Layer
  CHPP_TRANSPORT_ERROR_APPLAYER = 6,
  //! Timeout (implicit, deduced and used internally only)
  CHPP_TRANSPORT_ERROR_TIMEOUT = 0xF,
};

/**
 * Packet attributes in ChppTransportHeader (Most significant nibble of int8_t
 * packetCode).
 */
#define CHPP_TRANSPORT_ATTR_VALUE(value) (((value)&0x0f) << 4)
#define CHPP_TRANSPORT_ATTR_MASK MOST_SIGNIFICANT_NIBBLE
enum ChppTransportPacketAttributes {
  //! None
  CHPP_TRANSPORT_ATTR_NONE = CHPP_TRANSPORT_ATTR_VALUE(0),
  //! Reset
  CHPP_TRANSPORT_ATTR_RESET = CHPP_TRANSPORT_ATTR_VALUE(1),
  //! Reset Ack
  CHPP_TRANSPORT_ATTR_RESET_ACK = CHPP_TRANSPORT_ATTR_VALUE(2),
};

/**
 * CHPP Transport Layer header (not including the preamble)
 */
CHPP_PACKED_START
struct ChppTransportHeader {
  //! Flags bitmap, defined as CHPP_TRANSPORT_FLAG_...
  uint8_t flags;

  //! LS Nibble: Defined in enum ChppTransportErrorCode
  //! MS Nibble: Defined in enum ChppTransportPacketAttributes
  uint8_t packetCode;

  //! Next expected sequence number for a payload-bearing packet
  uint8_t ackSeq;

  //! Sequence number
  uint8_t seq;

  //! Payload length in bytes (not including header / footer)
  uint16_t length;

  //! Reserved
  uint16_t reserved;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * CHPP Transport Layer footer (containing the checksum)
 */
CHPP_PACKED_START
struct ChppTransportFooter {
  // Checksum algo TBD. Maybe IEEE CRC-32?
  uint32_t checksum;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

enum ChppRxState {
  //! Waiting for, or processing, the preamble (i.e. packet start delimiter)
  //! Moves to CHPP_STATE_HEADER as soon as it has seen a complete preamble.
  CHPP_STATE_PREAMBLE = 0,

  //! Processing the packet header. Moves to CHPP_STATE_PAYLOAD after processing
  //! the expected length of the header.
  CHPP_STATE_HEADER = 1,

  //! Copying the packet payload. The payload length is determined by the
  //! header.
  //! Moves to CHPP_STATE_FOOTER afterwards.
  CHPP_STATE_PAYLOAD = 2,

  //! Processing the packet footer (checksum) and responding accordingly. Moves
  //! to CHPP_STATE_PREAMBLE afterwards.
  CHPP_STATE_FOOTER = 3,
};

enum ChppResetState {
  CHPP_RESET_STATE_RESETTING = 0,  //! Reset in progress
  CHPP_RESET_STATE_NONE = 1,       //! Not in the middle of a reset
};

/**
 * Semantic Versioning system of CHRE.
 */
CHPP_PACKED_START
struct ChppVersion {
  //! Major version of (breaking changes).
  uint8_t major;

  //! Minor version (backwards compatible changes).
  uint8_t minor;

  //! Patch version (bug fixes).
  uint16_t patch;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Payload that is sent along reset and reset-ack packets. This may be used to
 * advertise the configuration parameters of this CHPP instance, and/or set the
 * configuration parameters of the remote side (TODO).
 */
CHPP_PACKED_START
struct ChppTransportConfiguration {
  //! CHPP transport version.
  struct ChppVersion version;

  //! Receive MTU size.
  uint16_t rxMtu;

  //! Max outstanding packet window size (1 for current implementation).
  uint16_t windowSize;

  //! Transport layer timeout in milliseconds (i.e. to receive ACK).
  uint16_t timeoutInMs;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

struct ChppRxStatus {
  //! Current receiving state, as described in ChppRxState.
  enum ChppRxState state;

  //! Location counter in bytes within each state. Must always be reinitialized
  //! to 0 when switching states.
  size_t locInState;

  //! Next expected sequence number (for a payload-bearing packet)
  uint8_t expectedSeq;

  //! Packet (error) code, if any, of the last received packet
  uint8_t receivedPacketCode;

  //! Location counter in bytes within the current Rx datagram.
  size_t locInDatagram;

  //! Last received ACK sequence number (i.e. next expected sequence number for
  //! an outgoing payload-bearing packet)
  uint8_t receivedAckSeq;
};

struct ChppTxStatus {
  //! Last sent ACK sequence number (i.e. next expected sequence number for
  //! an incoming payload-bearing packet)
  uint8_t sentAckSeq;

  //! Last sent sequence number (irrespective of whether it has been received /
  //! ACKed or not)
  uint8_t sentSeq;

  //! Does the transport layer have any packets (with or without payload) it
  //! needs to send out?
  bool hasPacketsToSend;

  //! Error code, if any, of the next packet the transport layer will send out.
  uint8_t packetCodeToSend;

  //! How many bytes of the front-of-queue datagram has been sent out
  size_t sentLocInDatagram;

  //! Note: For a future ACK window >1, sentLocInDatagram doesn't always apply
  //! to the front-of-queue datagram. Instead, we need to track the queue
  //! position the datagram being sent as well (relative to the front-of-queue).
  //! e.g. uint8_t datagramBeingSent

  //! How many bytes of the front-of-queue datagram has been acked
  size_t ackedLocInDatagram;

  //! Whether the link layer is still processing pendingTxPacket
  bool linkBusy;
};

struct PendingTxPacket {
  //! Length of outgoing packet to the Link Layer
  size_t length;

  //! Payload of outgoing packet to the Link Layer
  uint8_t payload[CHPP_LINK_TX_MTU_BYTES];
};

struct ChppDatagram {
  //! Length of datagram payload in bytes (A datagram can be constituted from
  //! one or more packets)
  size_t length;

  // Datagram payload
  uint8_t *payload;
};

struct ChppTxDatagramQueue {
  //! Number of pending datagrams in the queue.
  uint8_t pending;

  //! Index of the datagram at the front of the queue.
  uint8_t front;

  //! Location counter within the front datagram (i.e. the datagram at the front
  //! of the queue), showing how many bytes of this datagram have already been
  //! packetized and processed.
  size_t loc;

  //! Array of datagrams
  struct ChppDatagram datagram[CHPP_TX_DATAGRAM_QUEUE_LEN];
};

struct ChppTransportState {
  struct ChppAppState *appContext;  // Pointer to app layer context

  struct ChppRxStatus rxStatus;         // Rx state and location within
  struct ChppTransportHeader rxHeader;  // Rx packet header
  struct ChppTransportFooter rxFooter;  // Rx packet footer (checksum)
  struct ChppDatagram rxDatagram;       // Rx datagram

  struct ChppTxStatus txStatus;                // Tx state
  struct ChppTxDatagramQueue txDatagramQueue;  // Queue of datagrams to be Tx
  struct PendingTxPacket pendingTxPacket;      // Outgoing packet to Link Layer

  struct ChppMutex mutex;          // Lock for transport state (i.e. context)
  struct ChppNotifier notifier;    // Notifier for main thread
  enum ChppResetState resetState;  // Maintains state of a reset

  //! This MUST be the last field in the ChppTransportState structure, otherwise
  //! chppResetTransportContext() will not work properly.
  struct ChppPlatformLinkParameters linkParams;  // For corresponding link layer

  // !!! DO NOT ADD ANY NEW FIELDS HERE - ADD THEM BEFORE linkParams !!!
};

/************************************************
 *  Public functions
 ***********************************************/

/**
 * Initializes the CHPP transport layer state stored in the parameter
 * transportContext.
 * It is necessary to initialize state for each transport layer instance on
 * every platform.
 * Each transport layer instance is associated with a single application layer
 * instance. appContext points to the application layer status struct associated
 * with this transport layer instance.
 *
 * Note: It is necessary to initialize the platform-specific values of
 * transportContext.linkParams (prior to the call, if needed in the link layer
 * APIs, such as chppPlatformLinkInit()).
 *
 * @param transportContext Maintains status for each transport layer instance.
 * @param appContext The app layer status struct associated with this transport
 * layer instance.
 */
void chppTransportInit(struct ChppTransportState *transportContext,
                       struct ChppAppState *appContext);

/**
 * Deinitializes the CHPP transport layer and does necessary clean-ups for
 * e.g. clean shutdown.
 *
 * @param transportContext A non-null pointer to ChppTransportState
 * initialized previously in chppTransportInit().
 */
void chppTransportDeinit(struct ChppTransportState *transportContext);

/**
 * Processes all incoming data on the serial port based on the Rx state.
 * stream. Checks checksum, triggering the correct response (ACK / NACK).
 * Moves the state to CHPP_STATE_PREAMBLE afterwards.
 *
 * TODO: Add requirements, e.g. context must not be modified unless locked via
 * mutex.
 *
 * TODO: Add sufficient outward facing documentation
 *
 * @param context Maintains status for each transport layer instance.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 *
 * @return true informs the serial port driver that we are waiting for a
 * preamble. This allows the driver to (optionally) filter incoming zeros and
 * save processing
 */
bool chppRxDataCb(struct ChppTransportState *context, const uint8_t *buf,
                  size_t len);

/**
 * Callback function for the timer that detects timeouts during transmit.
 *
 * @param context Maintains status for each transport layer instance.
 */
void chppTxTimeoutTimerCb(struct ChppTransportState *context);

/**
 * Callback function for the timer that detects timeouts during receive.
 *
 * @param context Maintains status for each transport layer instance.
 */
void chppRxTimeoutTimerCb(struct ChppTransportState *context);

/**
 * Enqueues an outgoing datagram of a specified length and ffrees the payload
 * asynchronously after it is sent. The payload must have been allocated by the
 * caller using chppMalloc.
 *
 * If enqueueing a datagram is unsuccessful, the payload is freed (discarded)
 * and an error message printed.
 *
 * @param context Maintains status for each transport layer instance.
 * @param buf Datagram payload allocated through chppMalloc. Cannot be null.
 * @param len Datagram length in bytes.
 *
 * @return True informs the sender that the datagram was successfully enqueued.
 * False informs the sender that the queue was full and the payload discarded.
 */
bool chppEnqueueTxDatagramOrFail(struct ChppTransportState *context, void *buf,
                                 size_t len);

/**
 * Enables the App Layer to enqueue an outgoing error datagram, for example for
 * an OOM situation over the wire.
 *
 * @param context Maintains status for each transport layer instance.
 * @param packetCode Error code and packet attributes to be sent.
 */
void chppEnqueueTxErrorDatagram(struct ChppTransportState *context,
                                uint8_t packetCode);

/**
 * Starts the main thread for CHPP's Transport Layer. This thread needs to be
 * started after the Transport Layer is initialized through chppTransportInit().
 * Note that a platform may implement this as a new thread or as part of an
 * existing thread.
 *
 * If needed (e.g. for testing and debugging), this thread can be stopped by
 * calling chppWorkThreadStop().
 *
 * @param context Maintains status for each transport layer instance.
 */
void chppWorkThreadStart(struct ChppTransportState *context);

/**
 * Signals the main thread for CHPP's Transport Layer to perform some work. This
 * method should only be called from the link layer.
 *
 * @param params Platform-specific struct with link details / parameters.
 * @param signal The signal that describes the work to be performed. Only bits
 * specified by CHPP_TRANSPORT_SIGNAL_PLATFORM_MASK can be set.
 */
void chppWorkThreadSignalFromLink(struct ChppPlatformLinkParameters *params,
                                  uint32_t signal);

/**
 * Stops the main thread for CHPP's Transport Layer that has been started by
 * calling chppWorkThreadStart(). Stopping this thread may be necessary for
 * testing and debugging purposes.
 *
 * @param context Maintains status for each transport layer instance.
 */
void chppWorkThreadStop(struct ChppTransportState *context);

/*
 * Notifies the transport layer that the link layer is done sending the previous
 * payload (as provided to platformLinkSend() through buf and len) and can
 * accept more data.
 *
 * On systems that implement the link layer Tx asynchronously, where
 * platformLinkSend() returns False before consuming the payload provided to it
 * (i.e. buf and len), the platform implementation must call this function after
 * platformLinkSend() is done with the payload (i.e. buf and len).
 *
 * @param params Platform-specific struct with link details / parameters.
 * @param error Indicates success or failure type.
 */
void chppLinkSendDoneCb(struct ChppPlatformLinkParameters *params,
                        enum ChppLinkErrorCode error);

/*
 * Notifies the transport layer that the app layer is done with the previous
 * payload (as provided to chppProcessRxDatagram() through buf and len), so it
 * is freed appropriately etc.
 *
 * TODO: Look into automatically doing this when a response is sent back by a
 * service.
 *
 * @param context Maintains status for each transport layer instance.
 * @param buf Pointer to the buf given to chppProcessRxDatagram. Cannot be null.
 */
void chppAppProcessDoneCb(struct ChppTransportState *context, uint8_t *buf);

/**
 * Sends a reset or reset-ack packet over the link in order to reset the remote
 * side or inform the counterpart of a reset, respectively. The transport
 * layer's configuration is sent as the payload of the reset packet.
 *
 * This function should only be used immediately after initialization, for
 * example upon boot (to send a reset), or when a reset packet is received and
 * acted upon (to send a reset-ack).
 *
 * @param transportContext Maintains status for each transport layer instance.
 * @param resetType Distinguishes a reset from a reset-ack, as defined in the
 * ChppTransportPacketAttributes struct.
 */
void chppTransportSendReset(struct ChppTransportState *context,
                            enum ChppTransportPacketAttributes resetType);

#ifdef __cplusplus
}
#endif

#endif  // CHPP_TRANSPORT_H_
