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
#include "driver/system.h"
#include "audio.h"
#include "driver/eeprom.h"

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
uint8_t     gDTMFDIGI_ringPulse=1;              // Ring pulse duration
uint16_t    gDTMFDIGI_ringTimer;
uint16_t    gDTMFDIGI_ringPulseTimer;
bool        gDTMFDIGI_ringing = false;
bool        gDTMFDIGI_updRing;
bool        gDTMFDIGI_ringPulseOn;
bool        gDTMFDIGI_endRing;
bool        gDTMFDIGI_answered;
bool        gDTMFDIGI_terminate;
bool        gDTMFDIGI_sendACK;                  // If true, overrides pending packet and sends ACK
uint8_t     gDTMFDIGI_otherRadio;               //
bool        gDTMFDIGI_waitACK;                  // If true, waits for ACK
bool        gDTMFDIGI_softReset;
bool        gDTMFDIGI_missedCall;
uint16_t    gMyANI;
bool        gDTMFDIGI_init=true;
DTMFDIGI_contact    contactList[MAX_DTMF_CONTACTS];

const char          DTMFCHARS[]="0123456789ABCD*#";
DTMFDIGI_calltype   gDTMFDIGI_callReg[16];
uint8_t             gDTMFDIGI_callRegSize;

/*uint16_t GetMyANI()
{
    return DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[0])*100+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[1])*10+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[2]);
}*/

void DTMFDIGI_InitPhoneBook()
{
    char temp[4];
    for (uint8_t i = 0; i < MAX_DTMF_CONTACTS; i++)
    {
	    EEPROM_ReadBuffer(0x1C00 + (i * 16), contactList[i].contactName, 8);
        contactList[i].contactName[8] = '\0';
        EEPROM_ReadBuffer(0x1C08 + (i * 16), temp, 3);
        contactList[i].contactId = DTMFDGI_DTMFToNibble(temp[0])*100 + DTMFDGI_DTMFToNibble(temp[1])*10 + DTMFDGI_DTMFToNibble(temp[2]);

        contactList[i].isNull = contactList[i].contactName[0] == '\0';
    }
}

void DTMFDIGI_SetRing()
{
    gDTMFDIGI_ringing = true;
    gDTMFDIGI_ringPulseOn = true;
    gDTMFDIGI_ringTimer = gDTMFDIGI_ring*100;           // DTMFDIGI_Process() is executed exactly every 10ms
    gDTMFDIGI_ringPulseTimer = gDTMFDIGI_ringPulse*100;
    gDTMFDIGI_updRing = true;
    gDTMFDIGI_endRing = false;  
}

void DTMFDIGI_SendCALLST()
{
    gDTMFDIGI_Packet.dataType=PACKET_TYPE_CALLST;
    gDTMFDIGI_Packet.receiverId=gDTMFDIGI_caller;
    gDTMFDIGI_Packet.data[0] = gDTMFDIGI_callStatus | (gDTMFDIGI_callStatus==CALL_STATUS_RINGING)?(gDTMFDIGI_ring << 2):0x00;
    //gDTMFDIGI_Packet.data[1] = gDTMFDIGI_ring;
    /*if (gDTMFDIGI_callStatus == CALL_STATUS_RINGING)
        gDTMFDIGI_Packet.data[0] = gDTMFDIGI_callStatus | (gDTMFDIGI_ring << 2);*/
    gDTMFDIGI_Packet.dataLength = 1;
    
    DTMFDIGI_GenerateSend();
    gDTMFDIGI_waitACK = true;
}

void DTMFDIGI_SendACK(uint8_t receiver)
{
    gDTMFDIGI_Packet.dataType=PACKET_TYPE_ACK;
    gDTMFDIGI_Packet.dataLength=0;
    gDTMFDIGI_Packet.receiverId=receiver;
    DTMFDIGI_GenerateSend();
}

void DTMFDIGI_GenerateSend(void)
{
    gDTMFDIGI_Packet.senderId=gMyANI;
    gDTMFDIGI_RawPacket_length=DTMFDGI_generateRawPacket(&gDTMFDIGI_Packet, gDTMFDIGI_RawPacket);
    DTMFDIGI_SendRawPacket();
}

void DTMFDIGI_SendRawPacket()
{
    
    RADIO_PrepareTX();
    SYSTEM_DelayMs(400);
    BK4819_EnterDTMF_TX(false);
    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length] = '\0';
    BK4819_PlayDTMFString(gDTMFDIGI_RawPacket, true, 100, 100, 100, 100);
    BK4819_ExitDTMF_TX(true);
    FUNCTION_Select(FUNCTION_RECEIVE);
    SYSTEM_DelayMs(400);
}

#ifdef ENABLE_DTMFDIGI_DEBUG
void DTMFDIGI_Proces_DEBUG(KEY_Code_t Key)
{
    if (Key == KEY_EXIT)
        DTMFDIGI_RestoreDisplay();
    else if (Key == KEY_STAR)
    {
        DTMFDIGI_Init();
    }
    else if (Key == KEY_0)
        DTMFDIGI_SetRing();
}
#endif

void DTMFDIGI_Proces_MAIN(KEY_Code_t Key)
{
    if (Key == KEY_UP)
    {
        if (DTMFDIGI_menuItem == 0)
            DTMFDIGI_menuItem = DTMFDIGI_MENU_LAST-1;
        else
            DTMFDIGI_menuItem--;
        
        DTMFDIGI_updateDisplay = true;
    }
    else if (Key == KEY_DOWN)
    {
        DTMFDIGI_menuItem = (DTMFDIGI_menuItem+1 == DTMFDIGI_MENU_LAST) ? (0) : (DTMFDIGI_menuItem+1);
        /*if (DTMFDIGI_MENU_ITEMS[DTMFDIGI_menuItem] == NULL)
            DTMFDIGI_menuItem = 0;*/

        DTMFDIGI_updateDisplay = true;
    }
    else if (Key == KEY_MENU)
    {
        if (DTMFDIGI_menuItem == DTMFDIGI_MENU_CALL)
            DTMFDIGI_SwitchDisplay(DTMFDIGI_DISPL_CONTACTS, DTMFDIGI_DISPL_MAIN);
    #ifdef ENABLE_DTMFDIGI_DEBUG
        else if (DTMFDIGI_menuItem == DTMFDIGI_MENU_DEBUG)
            DTMFDIGI_SwitchDisplay(DTMFDIGI_DISPL_DEBUG,DTMFDIGI_DISPL_MAIN);
    #endif
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

    #ifdef ENABLE_DTMFDIGI_DEBUG
    DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_DEBUG);
    #endif
    if (gDTMFDIGI_request_stage != 0x02)
        return;
    
    gDTMFDIGI_request_stage = 0;

    // Now we can decode the raw packet into the DTMF_Packet structure
    gDTMFDIGI_Packet.processed = false;              //Generate Packet processed IRQ    if (gDTMFDIGI_RawPacket_length < 10)

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
        case PACKET_TYPE_ACK:
        case PACKET_TYPE_CALLRQ:
        case PACKET_TYPE_BADPKT:
        case PACKET_TYPE_MSGRF:
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
        default:
            gDTMFDIGI_Packet.error = PACKET_ERROR_INVALID_TYPE;
            break;
    }
    gDTMFDIGI_Packet.checksumt = myChecksum;
    //gDTMFDIGI_RawPacket_length = 0;

    if (gDTMFDIGI_Packet.error == PACKET_ERROR_NONE)
    {
        if (gDTMFDIGI_Packet.checksum != myChecksum)
            gDTMFDIGI_Packet.error = PACKET_ERROR_CHECKSUM;
    }

    if (gDTMFDIGI_Packet.receiverId != gMyANI)      // If the receiverid is different than the radio ani-id, we are done
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
                //gDTMFDIGI_Packet.processed = true;

                //FUNCTION_Select(FUNCTION_RECEIVE);
            }
        }
        else if (gDTMFDIGI_waitACK)
        {
            if (gDTMFDIGI_Packet.dataType == PACKET_TYPE_ACK)
                gDTMFDIGI_waitACK = false;
        }
        

        return;
    }
    
    // Error management
}
    
void DTMFDIGI_HandleRequest(void){
    if (gDTMF_RX_pending && !gDTMFDIGI_standard_handle)
    {
        #ifdef ENABLE_DTMFDIGI_DEBUG
        DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_DEBUG);
        #endif
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

    //DTMF_HandleRequest(); // call the standard DTMF handle function
}


void DTMFDIGI_ForceCALLSCREEN(void)
{
    //DTMFDIGI_InitDisplay();
    GUI_SelectNextDisplay(DISPLAY_DTMFDIGI);
    DTMFDIGI_SwitchDisplay(DTMFDIGI_DISPL_CALLSCR, DTMFDIGI_DISPL_MAIN);
    //GUI_DisplayScreen();
}

void DTMFDIGI_Process(void)
{
    if (gDTMFDIGI_init)
        DTMFDIGI_Init();

    if (gDTMFDIGI_ringing)
    {
        // Ringing code
        if (--gDTMFDIGI_ringPulseTimer==0)
        {
            gDTMFDIGI_ringPulseTimer = gDTMFDIGI_ringPulse*100;
            gDTMFDIGI_ringPulseOn = !gDTMFDIGI_ringPulseOn;

            gDTMFDIGI_updRing = true;
        }

        if (--gDTMFDIGI_ringTimer==0)
        {
            gDTMFDIGI_endRing = true;
        }

        if (gDTMFDIGI_updRing || gDTMFDIGI_endRing)
        {
            if (!gDTMFDIGI_ringPulseOn && !gDTMFDIGI_endRing)
            {
                /*AUDIO_AudioPathOn();
                BK4819_PlayTone(880, true);
                BK4819_ExitTxMute();*/
			    AUDIO_PlayBeep(BEEP_880HZ_500MS);
                gDTMFDIGI_ringTimer-=50;
                gDTMFDIGI_ringPulseTimer-=50;
                
            }
            else if (gDTMFDIGI_ringPulseOn || gDTMFDIGI_endRing )
            {
                /*BK4819_EnterTxMute();
                AUDIO_AudioPathOff();
                BK4819_TurnsOffTones_TurnsOnRX();*/
            }

            if (gDTMFDIGI_endRing)
            {
                //gDTMFDIGI_endRing = false;
                gDTMFDIGI_ringing = false;
            }

            gDTMFDIGI_updRing = false;
        }
        
    }
    
    if (forceExit)
    {
        //gScreenToDisplay = DISPLAY_MAIN;
        GUI_SelectNextDisplay(DISPLAY_MAIN);
        forceExit = false;
    }

    if (gDTMFDIGI_comm_status == COMM_STATUS_CLOSED && gDTMFDIGI_callStatus != CALL_STATUS_RINGING)
        gDTMFDIGI_callStatus = CALL_STATUS_RINGING;

    if (gCurrentFunction == FUNCTION_TRANSMIT)
        return;

    #ifdef ENABLE_DTMFDIGI_DEBUG
    DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_DEBUG);
    #endif

    if (gDTMFDIGI_waitACK)
        return;


    if (gDTMFDIGI_sendACK)
    {
        gDTMFDIGI_sendACK = false;
        DTMFDIGI_SendACK(gDTMFDIGI_otherRadio);
    }
    else if (gDTMFDIGI_softReset)
    {
        DTMFDIGI_Init();
    }
    else
    {
        if (gDTMFDIGI_comm_status == COMM_STATUS_CALLREQ_IN)
        {
            if (!gDTMFDIGI_Packet.processed)
            {
                if (gDTMFDIGI_Packet.dataType == PACKET_TYPE_ACK)
                {
                    gDTMFDIGI_comm_status = COMM_STATUS_CALL_IN;
                    gDTMFDIGI_Packet.processed = true;
                    DTMFDIGI_SetRing();
                    DTMFDIGI_ForceCALLSCREEN();
                    return;
                }
            }
            // We're accepting the call, so send CALLST=RING
            gDTMFDIGI_callStatus = CALL_STATUS_RINGING;
            
            DTMFDIGI_SendCALLST();
        }
        else if (((gDTMFDIGI_comm_status == COMM_STATUS_CALL_IN)||(gDTMFDIGI_comm_status == COMM_STATUS_CALL_OUT)))
        {
            if (gDTMFDIGI_terminate)
                gDTMFDIGI_callStatus = CALL_STATUS_TERMINATED;

            if (gDTMFDIGI_callStatus == CALL_STATUS_TERMINATED)
            {
                if (gDTMFDIGI_terminate)
                {
                    // Call terminated by us
                    if (!gDTMFDIGI_Packet.processed)
                    {
                        if (gDTMFDIGI_Packet.dataType == PACKET_TYPE_ACK)
                        {
                            DTMFDIGI_Init();
                            return;
                        }
                    }
                    DTMFDIGI_SendCALLST();
                }
                else
                {
                    gDTMFDIGI_sendACK = true;
                    gDTMFDIGI_softReset = true;
                }
            }
            // Call terminated            
        }
        
        if (gDTMFDIGI_comm_status == COMM_STATUS_CALL_IN)
        {
            if ( gDTMFDIGI_callStatus == CALL_STATUS_BUSY)
            {
                if (!gDTMFDIGI_Packet.processed && gDTMFDIGI_Packet.dataType == PACKET_TYPE_ACK)
                {
                    DTMFDIGI_Init();
                    return;
                }
                DTMFDIGI_SendCALLST();
            }
            else if (gDTMFDIGI_callStatus == CALL_STATUS_RINGING)
            {
                if ( gDTMFDIGI_endRing )    // Ring timeout
                    DTMFDIGI_Init();
                else if ( gDTMFDIGI_answered )
                {
                    gDTMFDIGI_callStatus = CALL_STATUS_OPEN;
                }
            }
            else if (gDTMFDIGI_callStatus == CALL_STATUS_OPEN && gDTMFDIGI_answered)
            {
                if (!gDTMFDIGI_Packet.processed && gDTMFDIGI_Packet.dataType == PACKET_TYPE_ACK)
                {
                    gDTMFDIGI_answered = false;
                    gDTMFDIGI_Packet.processed = true;
                    return;
                }

                DTMFDIGI_SendCALLST();
            }
        }
    }
    
}


void DTMFDIGI_BackgroundKeyProcess(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if ( !bKeyPressed && !bKeyHeld )
    {
        if ((gDTMFDIGI_callStatus == CALL_STATUS_RINGING) && (gDTMFDIGI_comm_status == COMM_STATUS_CALL_IN))
        {
            if (Key == KEY_PTT)
            {
                // ANSWER
                gDTMFDIGI_answered = true;
                gDTMFDIGI_ringing = false;
                return;
            }
            else if (Key == KEY_EXIT)
            {
                // CLOSE CALL - Busy
                gDTMFDIGI_callStatus = CALL_STATUS_BUSY;
                gDTMFDIGI_ringing = false;
                DTMFDIGI_RestoreDisplay();
                return;
            }
        }
        else if ((gDTMFDIGI_callStatus == CALL_STATUS_OPEN) && ((gDTMFDIGI_comm_status == COMM_STATUS_CALL_IN)||(gDTMFDIGI_comm_status == COMM_STATUS_CALL_OUT)))
        {
            if (Key == KEY_EXIT)
            {
                // Terminate call
                gDTMFDIGI_terminate = true;
                DTMFDIGI_RestoreDisplay();
                return;
            }
        }
    }
}
void DTMFDIGI_Process_CONTACTS(KEY_Code_t Key)
{
    if (Key == KEY_UP)
    {
        DTMFDIGI_menuItem = (DTMFDIGI_menuItem == 0) ? MAX_DTMF_CONTACTS-1 : DTMFDIGI_menuItem-1;
        DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_CONTACTS);
    }
    else if (Key == KEY_DOWN)
    {
        DTMFDIGI_menuItem = (DTMFDIGI_menuItem == MAX_DTMF_CONTACTS-1) ? 0 : DTMFDIGI_menuItem+1;
        DTMFDIGI_ForceUpdate(DTMFDIGI_DISPL_CONTACTS);
    }
    else if (Key == KEY_EXIT)
    {
        DTMFDIGI_RestoreDisplay();
    }
}

void DTMFDIGI_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld){
    // Call key handling
    // This code is here for testing purposes only, it should be moved where it can always be executed
    
    if ( DTMFDIGI_displayn == DTMFDIGI_DISPL_CALLSCR )
    {
        DTMFDIGI_BackgroundKeyProcess(Key, bKeyPressed, bKeyHeld);
        return;
    }
    // App key handling
    if ( !bKeyPressed && !bKeyHeld )
    {
        switch(DTMFDIGI_displayn)
        {
            case DTMFDIGI_DISPL_MAIN:
                DTMFDIGI_Proces_MAIN(Key);
                break;
            case DTMFDIGI_DISPL_CONTACTS:
                DTMFDIGI_Process_CONTACTS(Key);
                break;
            #ifdef ENABLE_DTMFDIGI_DEBUG
            case DTMFDIGI_DISPL_DEBUG:
                DTMFDIGI_Proces_DEBUG(Key);
                break;
            #endif
        }
    }
    else if ( bKeyPressed && bKeyHeld && (Key == KEY_EXIT))
    {
        forceExit = true;
    }
}

char DTMFDGI_nibbleToDTMF(uint8_t nibble)
{
    // Convert a nibble (4 bits) to a DTMF tone character
    //nibble &= 0x0F; // Ensure nibble is only 4 bits

    return DTMFCHARS[nibble & 0x0F];
}

uint8_t DTMFDGI_DTMFToNibble(char dtmf)
{
    if (dtmf >= '0' && dtmf <= '9')
        return (uint8_t)(dtmf - '0'); // 0-9

    switch (dtmf)
    {
        case '*': return 0x0E; 
        case '#': return 0x0F; 
        default: return (uint8_t)(dtmf-'A')+10;   
    }
}

uint8_t DTMFDGI_generateRawPacket(DTMF_Packet *packet, char* rawPacket)
{
    // Generate raw DTMF packet based on the protocol described above
    // This function will return a char array containing the DTMF tones to be sent

    packet->processed=true;
    uint8_t packetLength = 10; // Initialize packet length
    //char packet[80]; // Maximum packet length is 80 DTMF tones (40 bytes)
    rawPacket[0] = '*'; // Recognition sequence
    rawPacket[1] = '#'; // Recognition sequence

    // Add the data type to the packet
    rawPacket[2] = DTMFDGI_nibbleToDTMF(packet->dataType); // Data type is 4 bits (1 nibble)

    packet->checksum = 0x0E + 0x0F+(packet->dataType&0x0F); // Initialize checksum with recognition sequence and data type

    // Add the sender and receiver IDs to the packet
    uint32_t combinedIds = ((uint32_t)(packet->senderId & 0x3FF) << 10) | (uint32_t)(packet->receiverId & 0x3FF);
    uint8_t nibble;

    for (uint8_t i = 0; i < 5; i++) // 5 nibbles for IDs
    {
        nibble = (combinedIds >> (4 * (4 - i))) & 0x0F;
        packet->checksum += nibble;
        rawPacket[3 + i] = DTMFDGI_nibbleToDTMF(nibble);
    }

    // Put data in raw packet
    for (uint8_t i = 0; i < packet->dataLength; i++)
    {
        rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[i]);
        packet->checksum+= packet -> data[i];
    }

    /*
    uint8_t dataIndex = 0; // Index for the data array

    switch (packet->dataType)
    {
        case PACKET_TYPE_MSGRQ: // Message send request
            // Generate a message send request packet
            // data[0] = number of packets (4 bits)
            // data[1] = total length of the message (1 byte)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[0]); // Number of packets (4 bits)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[1]>>4); // Total length of the message (1 byte)
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[1]&0x0F); // Total length of the message (1 byte)
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
            rawPacket[packetLength++] = DTMFDGI_nibbleToDTMF(packet->data[0]); // Add the packet number (4 bits) to the packet
            packet->checksum += (packet->data[0] & 0x0F); // Update checksum with the data added
            break;
    }*/

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
    //DTMFDIGI_Init();

    // Check for errors
    if ((gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation == MODULATION_CW) || (gEeprom.VfoInfo[gEeprom.TX_VFO].Modulation == MODULATION_UKNOWN))
        DTMFDIGI_displayn = DTMFDIGI_DISPL_MODERR;
    else if (TX_freq_check(gCurrentVfo->pTX->Frequency)!=0)
        DTMFDIGI_displayn = DTMFDIGI_DISPL_TXERR;
    else if (gCurrentVfo->DTMF_DECODING_ENABLE == 0)
        DTMFDIGI_displayn = DTMFDIGI_DISPL_DTMFERR;
}

void DTMFDIGI_Init(void)
{
    gDTMFDIGI_init = false;
    gDTMFDIGI_comm_status = COMM_STATUS_CLOSED; // No communication in or out
    gDTMFDIGI_callStatus = CALL_STATUS_RINGING;
    gDTMFDIGI_sendACK = false;
    gDTMFDIGI_waitACK = false;
    gDTMFDIGI_ringing = false;
    gDTMFDIGI_answered = false;
    gDTMFDIGI_terminate = false;
    gDTMFDIGI_softReset = false;
    forceExit = false;
    gMyANI = DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[0])*100+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[1])*10+DTMFDGI_DTMFToNibble(gEeprom.ANI_DTMF_ID[2]);
    DTMFDIGI_InitPhoneBook();
}
#endif