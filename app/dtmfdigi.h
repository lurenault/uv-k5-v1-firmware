#ifndef APP_DTMFDIGI_H
#define APP_DTMFDIGI_H
#endif

#ifdef ENABLE_DTMF_DIGITAL

#include "driver/keyboard.h"

// Definitions for DTMF Digital Protocol
// Packet types
#define PACKET_TYPE_CALLRQ  0x00
#define PACKET_TYPE_MSGRQ   0x01
#define PACKET_TYPE_ACK     0x02
#define PACKET_TYPE_MSG     0x03
#define PACKET_TYPE_CALLST  0x04
#define PACKET_TYPE_BADPKT  0x05
#define PACKET_TYPE_MSGRF   0x06

// Call status codes
#define CALL_STATUS_RINGING     0x00
#define CALL_STATUS_BUSY        0x01
#define CALL_STATUS_TERMINATED  0x02

// Error codes
#define PACKET_ERROR_NONE            0x00
#define PACKET_ERROR_CHECKSUM        0x01
#define PACKET_ERROR_INVALID_TYPE    0x02
#define PACKET_ERROR_INVALID_LENGTH  0x03

#define DTMF_CHECKSUM_POS       8 // Position of the checksum in the packet (after the IDs and data)

// Packet structure:
typedef struct {
    uint8_t dataType;      // 1 byte: Packet type
    uint16_t senderId;     // 2 bytes: Sender ID (10 bits)
    uint16_t receiverId;   // 2 bytes: Receiver ID (10 bits)
    uint8_t data[34];      // Up to 34 bytes of data (message content or other information)
    uint8_t checksum;      // 1 byte: Checksum
    uint8_t checksumt;
    uint8_t dataLength;    // 1 byte: Length of the data
    uint8_t packet_num;     // Packet number in multiple packets communication
    bool processed;        // Flag to indicate if the packet has been processed
    uint8_t error;         // 0x00 - No error, 0x01 - Checksum error, 0x02 - Invalid packet type, 0x03 - Invalid data length
} DTMF_Packet;

extern char         gDTMFDIGI_RawPacket[80];    // Buffer for received DTMF tones
extern uint8_t      gDTMFDIGI_RawPacket_length; // Length of the received DTMF tones
extern DTMF_Packet  gDTMFDIGI_Packet;           // Structure to hold the parsed packet data
extern uint8_t      gDTMFDIGI_request_stage;
extern bool         gDTMFDIGI_standard_handle;  // Flag to indicate if standard DTMF handling should be used

void DTMFDIGI_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
uint8_t DTMFDGI_generateRawPacket(DTMF_Packet *packet, char* rawPacket);
char DTMFDGI_nibbleToDTMF(uint8_t nibble);
void APP_RunDTMFDigi(void);
void DTMFDIGI_HandleRequest(void);
uint8_t DTMFDGI_DTMFToNibble(char dtmf);
void DTMFDIGI_DecodePacket();

#endif