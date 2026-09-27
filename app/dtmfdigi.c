/*
    Protocol details

    This transmissin protocol uses DTMF tones to send data/commands between two radios.
    Basically each DTMF tone represents a nibble, so 2 DTMF tones represent a byte.

    The protocol is designed to be simple and robust, with a focus on error detection and correction.

    First of all, the sender must send a recognition sequence (0xEF or "*#") to let the receiver know
    that the sender is using this specific protocol and that the receiver should start listening for a
    data packet. The recognition sequence is followed by the data packet header, which contains the
    length of the data packet, the type of data being sent, and a checksum for error detection.

    LIST OF DATA TYPES:
    0x00 - Call Request: The sender is requesting to initiate a call (it has to send the caller id
                         and the callee id)
    0x01 - Message send request: The sender is requesting to send a message (it has to send the caller id and the callee id)
    0x02 - ACK: The sender is aknowledging the receipt of a data packet (it has to send the caller id and the callee id)
    0x03 - Message: The sender is sending a message (it has to send the caller id and the callee id)
    0x04 - Call Status: The sender is sending the status of a call (it has to send the caller id and the callee id). If
                        the sender is closing the call, no need for ACK from the receiver.
    0x05 - Bad packet: The sender is telling the receiver that the last packet was corrupted.
    0x06 - Message send refuse: The sender is refusing to receive a message (it has to send the caller id and the callee id)
    
    Data packet type lenght is always 1/2 byte (a DTMF tone).

    After that, the sender sends his id and the receiver id, combined in this way:
     bit 0-9: receiver id (10 bits)
     bit 10-19: sender id (10 bits)
    So we need exactly 20 bits to send the ids, which means 5 DTMF tones (5 nibbles) are needed to send the ids.

    After that the sender sends a 8 bit checksum (calculated considering also the header) and then it depends on the data type.
    Call request, Message send refuse and ACK do not need any further data.

    A message send request needs to send the number of packets that will be sent(4 bits) and the total length of the message (1 byte).
    A message data packet needs to send the packet number (4 bits), the length of the packet (also 4 bits) and the data (up to 16 bytes, so 32 DTMF tones).
    A Call status packet needs to send the status of the call (2 bits) and some other information depending on the status.

    CALL STATUS CODES:
    0x00 - Ringing for x seconds: The sender(the callee) is telling the receiver(the caller) that the call has been started
                                  and that it's ringing for x(max 6 bits) seconds. After that the call status will be declared as busy.
    0x01 - Busy: The sender(the callee) is telling the receiver(the caller) that the call has been rejected because the callee is busy.
    0x02 - Call terminated: The sender is telling the receiver that the call has been terminated.

    So in case call status is 0x00, the sender will combine the call status code and the number of seconds in this way:
     bit 0-1: call status code (2 bits)
     bit 2-7: number of seconds (6 bits)

    Based on this protocol, the maximum length of a packet(including the header) is 40 bytes, which means 80 DTMF tones.
    
    NOTE: The checksum is calculated by summing all the nibbles of the packet, including the recognition sequence and the data type, and then taking the result modulo 256. The checksum is then sent as a single byte (2 DTMF tones) at the end of the packet.
    */

#ifdef ENABLE_DTMF_DIGITAL

#include "app/dtmfdigi.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include "app/dtmf.h"
#include "driver/bk4819.h"
#include "ui/dtmfdigi.h"
#include <string.h>
#include <stdlib.h>  // abs()

uint8_t     gDTMFDIGI_request_stage = 0;        // 0: waiting for first recognition tone, 1: waiting for second recognition tone, 2: waiting for data packet
bool        gDTMFDIGI_standard_handle = false;  // true: use standard DTMF handle, false: use Digital DTMF handle
char        gDTMFDIGI_RawPacket[80];            // Buffer for received DTMF tones
uint8_t     gDTMFDIGI_RawPacket_length = 0;     // Length of the received DTMF tones
DTMF_Packet gDTMFDIGI_Packet;                   // Structure to hold the parsed packet data
uint8_t     gDTMFDIGI_caller;                   // Caller ID
uint8_t     gDTMFDIGI_callee;                   // Callee ID
bool        forceExit = false;                  // If set exits the app
uint8_t     gDTMFDIGI_comm_status;              // Communication status (See dtmfdigi.h)
uint8_t     gDTMFDIGI_msgLen;                   // Message length
uint8_t     gDTMFDIGI_msgPackets;               // Message packets
bool        gDTMFDIGI_receiveEN = true;         // Packet reception enabled
uint8_t     gDTMFDIGI_callStatus;               // Call status
uint8_t     gDTMFDIGI_ring=30;                  // Ringtone duration
bool        gDTMFDIGI_sendACK;                  // If true, overrides pending packet and sends ACK
uint8_t     gDTMFDIGI_otherRadio;               //

uint16_t GetMyANI()
{
    return DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[0])*100+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[1])*10+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[2]);
}

void DTMFDIGI_SendCALLST()
{
    gDTMFDIGI_Packet.dataType=PACKET_TYPE_CALLST;
    gDTMFDIGI_Packet.senderId=GetMyANI();
    gDTMFDIGI_Packet.receiverId=gDTMFDIGI_caller;
    gDTMFDIGI_Packet.data[0] = gDTMFDIGI_callStatus;
    if (gDTMFDIGI_callStatus == CALL_STATUS_RINGING)
        gDTMFDIGI_Packet.data[0] = gDTMFDIGI_callStatus | (gDTMFDIGI_ring << 2);
    gDTMFDIGI_Packet.dataLength = 1;
    
    gDTMFDIGI_RawPacket_length = DTMFDGI_generateRawPacket(&gDTMFDIGI_Packet,gDTMFDIGI_RawPacket);
    DTMFDIGI_SendRawPacket();
}

void DTMFDIGI_SendACK(uint8_t receiver)
{
    gDTMFDIGI_Packet.dataType=PACKET_TYPE_ACK;
    gDTMFDIGI_Packet.receiverId=receiver;
    gDTMFDIGI_Packet.senderId=GetMyANI();
    gDTMFDIGI_RawPacket_length=DTMFDGI_generateRawPacket(&gDTMFDIGI_Packet, gDTMFDIGI_RawPacket);
    DTMFDIGI_SendRawPacket();
}

void DTMFDIGI_SendRawPacket()
{
    RADIO_PrepareTX();
    BK4819_EnterDTMF_TX(false);
    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length] = '\0';
    BK4819_PlayDTMFString(gDTMFDIGI_RawPacket, true, 200, 200, 200, 200);
    BK4819_ExitDTMF_TX(true);
    FUNCTION_Select(FUNCTION_RECEIVE);
}

void DTMFDIGI_Proces_DEBUG(KEY_Code_t Key)
{
    if (Key == KEY_EXIT)
        DTMFDIGI_RestoreDisplay();
    else if (Key == KEY_STAR)
    {
        APP_RunDTMFDigi();
        DTMFDIGI_SwitchDisplay(DTMFDIGI_DISPL_DEBUG, DTMFDIGI_DISPL_MAIN);
    }
}

void DTMFDIGI_Proces_MAIN(KEY_Code_t Key)
{
    if (Key == KEY_UP)
    {
        if (DTMFDIGI_menuItem == 0)
        {
            while (DTMFDIGI_menuItem < 64)
            {
                if (DTMFDIGI_MENU_ITEMS[DTMFDIGI_menuItem] == NULL)
                {
                    DTMFDIGI_menuItem--;
                    break;
                }
                DTMFDIGI_menuItem++;
            }
        }
        else
            DTMFDIGI_menuItem--;
        
        DTMFDIGI_updateDisplay = true;
    }
    else if (Key == KEY_DOWN)
    {
        DTMFDIGI_menuItem++;
        if (DTMFDIGI_MENU_ITEMS[DTMFDIGI_menuItem] == NULL)
            DTMFDIGI_menuItem = 0;

        DTMFDIGI_updateDisplay = true;
    }
    else if (Key == KEY_MENU)
    {
        if (DTMFDIGI_menuItem == 2)
            DTMFDIGI_SwitchDisplay(DTMFDIGI_DISPL_DEBUG,DTMFDIGI_DISPL_MAIN);
    }
    else if (Key == KEY_EXIT)
        forceExit = true;
}

void DTMFDIGI_DecodePacket()
{
    if (gDTMFDIGI_standard_handle)
    {
        gDTMFDIGI_standard_handle = false; // Reset the flag for standard DTMF handling
        return; // If standard handle is true, do not decode the packet
    }


    DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_DEBUG);
    if (gDTMFDIGI_request_stage != 0x02)
        return;
    
    gDTMFDIGI_request_stage = 0;

    // Now we can decode the raw packet into the DTMF_Packet structure
    gDTMFDIGI_Packet.processed = true;              //Generate Packet processed IRQ    if (gDTMFDIGI_RawPacket_length < 10)

    gDTMFDIGI_Packet.error = PACKET_ERROR_NONE;

    if (gDTMFDIGI_RawPacket_length > 80 || gDTMFDIGI_RawPacket_length<10)
    {
        gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_LENGTH;
        return;
    }

    gDTMFDIGI_Packet.dataType = DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[2]);
    uint8_t myChecksum = 0x0E + 0x0F + gDTMFDIGI_Packet.dataType, nibble;  // Variable for calculating checksum 
    uint32_t tempId = 0;
    for (uint8_t i = 0; i<5; i++)
    {
        nibble = DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[i+3]);
        myChecksum += nibble;
        tempId = ( tempId << 4 ) | nibble;
    }

    gDTMFDIGI_Packet.senderId = (tempId >> 10) & 0x3FF; // Extract sender ID (10 bits)
    gDTMFDIGI_Packet.receiverId = tempId & 0x3FF; // Extract receiver ID
    gDTMFDIGI_Packet.checksum = (DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[8]) << 4) | (DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[9]));

    switch (gDTMFDIGI_Packet.dataType)
    {
        case PACKET_TYPE_CALLRQ:
            break;
        case PACKET_TYPE_MSGRQ:
            if(gDTMFDIGI_RawPacket_length != 13 )
            {
                gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_LENGTH;
                return;
            }
            gDTMFDIGI_msgPackets = DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[10]);   // Number of packets
            gDTMFDIGI_msgLen = (DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[11]) << 4)|(DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[12]));
            myChecksum += gDTMFDIGI_msgPackets+DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[11])+DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[12]);
            break;
        case PACKET_TYPE_ACK:
            break;
        case PACKET_TYPE_MSG:
            if ( gDTMFDIGI_RawPacket_length < 14 ) // <14 instead of <13 makes it impossible to receive an empty packet
            {
                gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_LENGTH;
                return;
            }

            gDTMFDIGI_Packet.packet_num = DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[10]); // Get packet number
            gDTMFDIGI_Packet.msgDataLength = DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[11]); // Packet length
            
            if (gDTMFDIGI_RawPacket_length != 12 + gDTMFDIGI_Packet.msgDataLength)
            {
                gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_LENGTH;
                return;
            }

            //Update checksum based on data
            myChecksum += gDTMFDIGI_Packet.packet_num + gDTMFDIGI_Packet.msgDataLength;

            for (uint8_t i = 12; i<gDTMFDIGI_RawPacket_length; i++)
                myChecksum += DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[i]);
            break;
        case PACKET_TYPE_CALLST:
            if(gDTMFDIGI_RawPacket_length != 11)
            {
                gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_LENGTH;
                return;
            }
            myChecksum += DTMFDGI_DTMFToNibble(gDTMFDIGI_RawPacket[10]);
            break;
        case PACKET_TYPE_BADPKT:
            break;
        case PACKET_TYPE_MSGRF:
            break;
        default:
            gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_TYPE;
            break;
    }

    //gDTMFDIGI_RawPacket_length = 0;

    if (gDTMFDIGI_Packet.error == PACKET_ERROR_NONE)
    {
        if (gDTMFDIGI_Packet.checksum != myChecksum)
            gDTMFDIGI_Packet.error = PACKET_ERROR_CHECKSUM;
    }

    if (gDTMFDIGI_Packet.receiverId != GetMyANI())      // If the receiverid is different than the radio ani-id, we are done
        return;

    if (gDTMFDIGI_Packet.error == PACKET_ERROR_NONE)
    {
        // Update status and let specified functions take control
        if (gDTMFDIGI_comm_status == COMM_STATUS_CLOSED)
        {
            if (gDTMFDIGI_Packet.dataType == PACKET_TYPE_CALLRQ)
            {
                // Someone wants to call us
                gDTMFDIGI_caller = gDTMFDIGI_Packet.senderId;
                gDTMFDIGI_comm_status = COMM_STATUS_CALLREQ_IN;
                gDTMFDIGI_otherRadio = gDTMFDIGI_caller;
                gDTMFDIGI_sendACK = true;

                //FUNCTION_Select(FUNCTION_RECEIVE);
            }
        }
        

        return;
    }
    
    // Error management
}
    
void DTMFDIGI_HandleRequest(void){
    if (gDTMF_RX_pending && !gDTMFDIGI_standard_handle)
    {
        DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_DEBUG);
        switch (gDTMFDIGI_request_stage)
        {
            case 0: // waiting for first recognition tone
                gDTMFDIGI_Packet.processed = false;
                gDTMFDIGI_RawPacket_length = 0; // reset raw packet length
                if (gDTMF_RX[0] == '*')
                {
                    gDTMFDIGI_request_stage = 1; // first recognition tone received, waiting for second recognition tone
                    gDTMF_RX_pending = false; // reset pending flag to avoid calling the standard DTMF handle function
                    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length++] = gDTMF_RX[0]; // reset raw packet buffer
                }
                else
                {
                    gDTMFDIGI_standard_handle = true; // not a recognition tone, use standard DTMF handle
                    gDTMFDIGI_request_stage = 0; // reset request stage
                }
                return;
                break;
            case 1: // waiting for second recognition tone
                if (gDTMF_RX[1] == '#')
                {
                    gDTMFDIGI_request_stage = 2; // second recognition tone received, waiting for data packet
                    gDTMF_RX_pending = false; // reset pending flag to avoid calling the standard DTMF handle function
                    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length++] = gDTMF_RX[1]; // add second recognition tone to raw packet buffer}
                }
                else
                {
                    gDTMFDIGI_standard_handle = true; // not a recognition tone, use standard DTMF handle
                    gDTMFDIGI_request_stage = 0; // reset request stage
                    gDTMFDIGI_RawPacket_length = 0;
                }
                return;
                break;
            case 2: // waiting for data packet
                // Copy the received DTMF tones to the raw packet buffer
                gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length++] = gDTMF_RX[--gDTMF_RX_index]; // add data packet to raw packet buffer
                gDTMF_RX_pending = false; // reset pending flag to avoid calling the standard DTMF handle function
                return;
                break;
        }
    }

    DTMF_HandleRequest(); // call the standard DTMF handle function
}

void DTMFDIGI_Process(void)
{
    if (forceExit)
    {
        GUI_SelectNextDisplay(DISPLAY_MAIN);
        return;
    }

    if (gCurrentFunction == FUNCTION_TRANSMIT)
        return;

    if (gDTMFDIGI_sendACK)
    {
        gDTMFDIGI_sendACK = false;
        DTMFDIGI_SendACK(gDTMFDIGI_otherRadio);
    }
    else
    {
        if (gDTMFDIGI_comm_status == COMM_STATUS_CALLREQ_IN)
        {
            // We're accepting the call, so send CALLST=RING
            gDTMFDIGI_callStatus = CALL_STATUS_RINGING;
            gDTMFDIGI_comm_status = COMM_STATUS_CALL_IN;
            
            DTMFDIGI_SendCALLST();
        }
    }
    
}

void DTMFDIGI_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld){
    if ( !bKeyPressed && !bKeyHeld )
    {
        switch(DTMFDIGI_displayn)
        {
            case DTMFDIGI_DISPL_MAIN:
                DTMFDIGI_Proces_MAIN(Key);
                break;
            case DTMFDIGI_DISPL_DEBUG:
                DTMFDIGI_Proces_DEBUG(Key);
                break;
        }
    }
    else if ( bKeyPressed && bKeyHeld && (Key == KEY_EXIT))
    {
        GUI_SelectNextDisplay(DISPLAY_MAIN);
    }
}

char DTMFDGI_nibbleToDTMF(uint8_t nibble)
{
    // Convert a nibble (4 bits) to a DTMF tone character
    nibble &= 0x0F; // Ensure nibble is only 4 bits

    if (nibble < 10)
    {
        return (char)(nibble + '0'); // 0-9
    }
    else
    {
        switch (nibble)
        {
            case 0x0A: return 'A'; 
            case 0x0B: return 'B'; 
            case 0x0C: return 'C'; 
            case 0x0D: return 'D'; 
            case 0x0E: return '*'; 
            case 0x0F: return '#'; 
            default: return '?';   // Invalid nibble
        }
    }
}

uint8_t DTMFDGI_DTMFToNibble(char dtmf)
{
    // Convert a DTMF tone character to a nibble (4 bits)
    if (dtmf >= '0' && dtmf <= '9')
    {
        return (uint8_t)(dtmf - '0'); // 0-9
    }
    else
    {
        switch (dtmf)
        {
            case 'A': return 0x0A; 
            case 'B': return 0x0B; 
            case 'C': return 0x0C; 
            case 'D': return 0x0D; 
            case '*': return 0x0E; 
            case '#': return 0x0F; 
            default: return -1;   // Invalid DTMF tone
        }
    }
}

uint8_t DTMFDGI_generateRawPacket(DTMF_Packet *packet, char* rawPacket)
{
    // Generate raw DTMF packet based on the protocol described above
    // This function will return a char array containing the DTMF tones to be sent

    uint8_t packetLength = 10; // Initialize packet length
    //char packet[80]; // Maximum packet length is 80 DTMF tones (40 bytes)
    rawPacket[0] = '*'; // Recognition sequence
    rawPacket[1] = '#'; // Recognition sequence

    // Add the data type to the packet
    rawPacket[2] = DTMFDGI_nibbleToDTMF(packet->dataType); // Data type is 4 bits (1 nibble)

    packet->checksum = 0x0E + 0x0F+(packet->dataType&0x0F); // Initialize checksum with recognition sequence and data type

    // Add the sender and receiver IDs to the packet
    uint32_t combinedIds = ((uint32_t)(packet->senderId & 0x3FF) << 10) | (uint32_t)(packet->receiverId & 0x3FF);
    
    for (uint8_t i = 0; i < 5; i++) // 5 nibbles for IDs
    {
        uint8_t nibble = (combinedIds >> (4 * (4 - i))) & 0x0F;
        packet->checksum += nibble;
        rawPacket[3 + i] = DTMFDGI_nibbleToDTMF(nibble);
    }

    uint8_t dataIndex = 0; // Index for the data array
    switch (packet->dataType)
    {
        case PACKET_TYPE_MSGRQ: // Message send request
            // Generate a message send request packet
            // data[0] = number of packets (4 bits)
            // data[1] = total length of the message (1 byte)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[dataIndex++]); // Number of packets (4 bits)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[dataIndex]>>4); // Total length of the message (1 byte)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[dataIndex++]&0x0F); // Total length of the message (1 byte)
            packet->checksum += (packet->data[0] & 0x0F) + (packet->data[1] >> 4) + (packet->data[2] & 0x0F); // Update checksum with the data added
            break;
        case PACKET_TYPE_MSG: // Message
            // Generate a message packet
            // data[0] = packet number (4 bits)
            // data[n] = message data (already coded in our own alphabet)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[dataIndex++]); // Packet number (4 bits)
            for (uint8_t i = 0; i < packet->dataLength; i++)
            {
                rawPacket[packetLength++] = packet->data[dataIndex++]; // Message data
                packet->checksum += (DTMFDGI_DTMFToNibble(packet->data[i]) & 0x0F); // Update checksum with the data added
            }
            break;
        case PACKET_TYPE_CALLST: // Call Status
            // Generate a call status packet
            // data[0] = call status code (2 bits)
            // data[1] = number of seconds (6 bits) if call status code is 0x00
            uint8_t callStatusCode = packet->data[0] & 0x03; // Extract the call status code (2 bits)
            if (callStatusCode == CALL_STATUS_RINGING)
                callStatusCode |= (packet->data[1] & 0x3F) << 2; // Combine with the number of seconds (6 bits)

            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(callStatusCode >> 4); // Add the call status code (2 bits) and number of seconds (6 bits) to the packet
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(callStatusCode); // Add the call status code (2 bits) and number of seconds (6 bits) to the packet
            packet->checksum += (callStatusCode >> 4) + (callStatusCode & 0x0F); // Update checksum with the data added
            break;
        case PACKET_TYPE_BADPKT: // Bad packet
            // Generate a bad packet notification
            // data[0] = packet number (4 bits)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[dataIndex++]); // Add the packet number (4 bits) to the packet
            packet->checksum += (packet->data[0] & 0x0F); // Update checksum with the data added
            break;
    }

    rawPacket[DTMF_CHECKSUM_POS] = DTMFDGI_nibbleToDTMF(packet->checksum >> 4); // Add high nibble of checksum to the packet
    rawPacket[DTMF_CHECKSUM_POS + 1] = DTMFDGI_nibbleToDTMF(packet->checksum); // Add low nibble of checksum to the packet

    //return *packet; // Return the generated packet
    return packetLength; // Return the length of the generated packet
}
void APP_RunDTMFDigi(void)
{
    // Initialize DTMF Digital mode
    //BK4819_PlayDTMF('*'); // Send recognition sequence
    // Check if selected DTMF decode is enabled on selected channel

    forceExit = false;
    DTMFDIGI_InitDisplay();
    gDTMFDIGI_comm_status = COMM_STATUS_CLOSED; // No communication in or out
    gDTMFDIGI_sendACK = false;

    // Check for errors
    if ((gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation == MODULATION_CW) || (gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation == MODULATION_UKNOWN))
        DTMFDIGI_displayn = DTMFDIGI_DISPL_MODERR;
    else if (TX_freq_check(gCurrentVfo->pTX->Frequency)!=0)
        DTMFDIGI_displayn = DTMFDIGI_DISPL_TXERR;
    else if (gCurrentVfo->DTMF_DECODING_ENABLE == 0)
        DTMFDIGI_displayn = DTMFDIGI_DISPL_DTMFERR;
}
#endif