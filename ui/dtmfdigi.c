#ifdef ENABLE_DTMF_DIGITAL
//#include "settings.h"
#include "app/dtmf.h"
#include "ui/dtmfdigi.h"
#include "ui/helper.h"
//#include "ui/ui.h"
#include "app/dtmfdigi.h"
#include "driver/st7565.h"
#include <string.h>
#include <stdlib.h>  // abs()
#include "external/printf/printf.h"
#include "driver/system.h"
#include "misc.h"
#include "settings.h"

/*
    Main menu must be like this

        small menu item n-2
        small menu item n-1
        big menu item n
        small menu item n+1
        small menu item n+2
*/

bool        DTMFDIGI_updateDisplay  =   true;   // If set updates the display
uint8_t     DTMFDIGI_displayStatus  =   0x00;
uint8_t     DTMFDIGI_displayn;                  // See defines in ui/dtmfdigi.h
uint8_t     DTMFDIGI_prevdisplayn;              // Same as above
uint8_t     DTMFDIGI_menuItem;                  // Selected menu item

/*
    Menu items:

    1. Call
    2. SMS
*/

const char* DTMFDIGI_MENU_ITEMS[] =      // \0 terminated
{
    "Call",
    "Text message",
    "Debug",
    NULL
};

void UI_DisplayDTMFDigi(void)
{
    if (!DTMFDIGI_updateDisplay || forceExit)
        return;

    UI_DisplayClear(); // clear the display

    switch(DTMFDIGI_displayn)
    {
        case DTMFDIGI_DISPL_MAIN:
            DTMFDIGI_UpdateMenu();
            break;
        case DTMFDIGI_DISPL_DEBUG:
            DTMFDIGI_UpdateDebug();
            break;
        default:
            UI_PrintString("ERROR",0,LCD_WIDTH,1,8);
            switch(DTMFDIGI_displayn)
            {
                case DTMFDIGI_DISPL_MODERR:
                    UI_PrintStringSmallNormal("Voice TX disable",0,LCD_WIDTH,3);
                    break;
                case DTMFDIGI_DISPL_DTMFERR:
                    UI_PrintStringSmallNormal("DTMF disabled",0,LCD_WIDTH,3);
                    break;
                case DTMFDIGI_DISPL_TXERR:
                    UI_PrintStringSmallNormal("TX disabled",0,LCD_WIDTH,3);
                    break;
                default:
                    UI_PrintStringSmallNormal("Unsupported",0,LCD_WIDTH,3);
                    break;
            }
            forceExit = true;
    }
    
    char String[20], vfonam[18];
    sprintf(vfonam,"%s",gEeprom.VfoInfo[gEeprom.TX_VFO].Name);
    if (strcmp(vfonam,"") != 0)
        sprintf(String,"CH: %s",gEeprom.VfoInfo[gEeprom.TX_VFO].Name);
    else
    {
        sprintf(vfonam,"%d",gCurrentVfo->pTX->Frequency);
        memmove(&vfonam[4],&vfonam[3], strlen(vfonam));
        vfonam[3] = '.';
        sprintf(String,"FREQ: %s",vfonam);
    }
    UI_PrintStringSmallBold(String,2,0,6);
    ST7565_BlitFullScreen(); // update the display
    DTMFDIGI_updateDisplay = false;

    if (forceExit)
        SYSTEM_DelayMs(1500);
}

void DTMFDIGI_InitDisplay(void)
{
    DTMFDIGI_updateDisplay = true;
    DTMFDIGI_menuItem = 0;
    DTMFDIGI_displayn = DTMFDIGI_DISPL_MAIN;
}

void DTMFDIGI_SwitchDisplay(uint8_t display, uint8_t return_point)
{
    DTMFDIGI_InitDisplay();
    DTMFDIGI_displayn = display;
    DTMFDIGI_prevdisplayn = return_point;
}

void DTMFDIGI_RestoreDisplay(void)
{
    DTMFDIGI_InitDisplay();
    DTMFDIGI_displayn = DTMFDIGI_prevdisplayn;
}

void DTMFDIGI_UpdateMenu(void)
{
    uint8_t prevItem, nextItem = DTMFDIGI_menuItem+1;

    if ( DTMFDIGI_menuItem == 0)
    {
        prevItem = 0;
        while (prevItem < 64)
        {
            if (DTMFDIGI_MENU_ITEMS[prevItem] == NULL)
            {
                prevItem--;
                break;
            }
            prevItem++;
        }
    }
    else
        prevItem = DTMFDIGI_menuItem-1;

    if ( DTMFDIGI_MENU_ITEMS[nextItem] == NULL)
        nextItem = 0;

    UI_PrintStringSmallNormal(DTMFDIGI_MENU_ITEMS[prevItem],0, LCD_WIDTH, 1);
    UI_PrintString(DTMFDIGI_MENU_ITEMS[DTMFDIGI_menuItem],0,LCD_WIDTH,2,8);
    UI_PrintStringSmallNormal(DTMFDIGI_MENU_ITEMS[nextItem],0,LCD_WIDTH,4);    
}


void DTMFDIGI_UpdateDebug(void)
{/*
    UI_PrintString("DEBUG",0,LCD_WIDTH,0,8);
    if ((gDTMFDIGI_request_stage != 0x02) && (!gDTMFDIGI_Packet.processed))
    {
        UI_PrintStringSmallNormal("Wait for sync",2,0,2);
        return;
    }

    char String[20];

    if (gDTMFDIGI_Packet.processed)
    {
        if (gDTMFDIGI_Packet.error == PACKET_ERROR_NONE)
        {
            sprintf(String, "TYP: %02X CRC: %02X",gDTMFDIGI_Packet.dataType,gDTMFDIGI_Packet.checksum);
            UI_PrintStringSmallNormal(String,2,0,2);
            sprintf(String, "SND: %d REC: %d", gDTMFDIGI_Packet.senderId, gDTMFDIGI_Packet.receiverId);
            UI_PrintStringSmallNormal(String,2,0,3);
        }
        else
        {
            switch(gDTMFDIGI_Packet.error)
            {
                case PACKET_ERROR_CHECKSUM:
                    UI_PrintString("CRC ERR",0,LCD_WIDTH,2,8);
                    break;
                case PACKET_ERROR_INVALID_LENGTH:
                    UI_PrintString("LEN ERR",0,LCD_WIDTH,2,8);
                    break;
                case PACKET_ERROR_INVALID_TYPE:
                    UI_PrintString("TYP ERR",0,LCD_WIDTH,2,8);
            }
        }
    }
    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length] = '\0';
    UI_PrintStringSmallNormal(gDTMFDIGI_RawPacket,2,0,4);

    //String = "COMM"
*/}


void DTMFDIGI_ForceUpdate(uint8_t screen)
{
    if (screen == DTMFDIGI_displayn)
        DTMFDIGI_updateDisplay = true;
}

#endif