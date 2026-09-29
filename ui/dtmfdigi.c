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
uint8_t     DTMFDIGI_prevPrevDisplayN;
uint8_t     DTMFDIGI_menuItem;                  // Selected menu item
uint8_t     DTMFDIGI_selContact;

/*
    Menu items:

    1. Call
    2. SMS
*/

const char* DTMFDIGI_MENU_ITEMS[] =      // \0 terminated
{
    [DTMFDIGI_MENU_CALL] = "Call",
    [DTMFDIGI_MENU_TEXT] = "Text message",
#ifdef ENABLE_DTMFDIGI_DEBUG
    [DTMFDIGI_MENU_DEBUG] = "Debug",
#endif
    [DTMFDIGI_MENU_REG] = "Call register",
    [DTMFDIGI_MENU_LAST] = NULL
};

const char* DTMFDIGI_CONTACT_OPT[] =
{
    [DTMFDIGI_OPT_CALL] = "Call",
    [DTMFDIGI_OPT_MSG] = "Text Message",
    [DTMFDIGI_OPT_ID] = NULL
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
        #ifdef ENABLE_DTMFDIGI_DEBUG
        case DTMFDIGI_DISPL_DEBUG:
            DTMFDIGI_UpdateDebug();
            break;
        #endif
        case DTMFDIGI_DISPL_CALLSCR:
            DTMFDIGI_UpdateCallScr();
            break;
        case DTMFDIGI_DISPL_CONTACTS:
            DTMFDIGI_UpdatePhoneBook();
            break;
        case DTMFDIGI_DISPL_CONTOPT:
            DTMFDIGI_UpdContactOpt();
            break;
        default:
            UI_PrintString("ERROR",0,LCD_WIDTH,1,8);
            /*switch(DTMFDIGI_displayn)
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
            }*/
            forceExit = true;
    }
    
    char String[20];
    //sprintf(vfonam,"%s",gCurrentVfo->Name);
    if (gCurrentVfo->Name[0] != '\0')
        sprintf(String,"CH: %s",gCurrentVfo->Name);
    else
    {
        //sprintf(vfonam,"%d",gCurrentVfo->pTX->Frequency);
        //memmove(&vfonam[4],&vfonam[3], strlen(vfonam));
        //vfonam[3] = '.';
        sprintf(String,"FR: %s",UI_FormatFrequency(String,gCurrentVfo->pTX->Frequency,4,false));
    }
    
    UI_PrintStringSmallBold(String,2,0,6);
    ST7565_BlitFullScreen(); // update the display
    DTMFDIGI_updateDisplay = false;

    if (forceExit)
        SYSTEM_DelayMs(1500);
}

void DTMFDIGI_UpdContactOpt(void)
{
    UI_PrintStringSmallBold(contactList[DTMFDIGI_selContact].contactName,2,0,0);
    uint8_t prev = (DTMFDIGI_menuItem == 0) ? 2 : (DTMFDIGI_menuItem-1);
    uint8_t succ = (DTMFDIGI_menuItem == 2) ? 0 : (DTMFDIGI_menuItem+1);

    UI_PrintStringSmallNormal(DTMFDIGI_CONTACT_OPT[prev],0,LCD_WIDTH,1);
    UI_PrintString(DTMFDIGI_CONTACT_OPT[DTMFDIGI_menuItem],0,LCD_WIDTH,2,8);
    UI_PrintStringSmallNormal(DTMFDIGI_CONTACT_OPT[succ],0,LCD_WIDTH,1);
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

void DTMFDIGI_NextDisplay(uint8_t display)
{
    DTMFDIGI_InitDisplay();
    DTMFDIGI_prevPrevDisplayN = DTMFDIGI_prevdisplayn;
    DTMFDIGI_prevdisplayn = DTMFDIGI_displayn;
    DTMFDIGI_displayn = display;
}

void DTMFDIGI_RestoreDisplay(void)
{
    DTMFDIGI_InitDisplay();
    DTMFDIGI_displayn = DTMFDIGI_prevdisplayn;
    DTMFDIGI_prevPrevDisplayN = DTMFDIGI_prevdisplayn;
}

void DTMFDIGI_UpdatePhoneBook(void)
{   
    UI_PrintStringSmallNormal(contactList[DTMFDIGI_FindPrevContact()].contactName,0,LCD_WIDTH,1);
    UI_PrintString(contactList[DTMFDIGI_menuItem].contactName,0,LCD_WIDTH,2,8);
    UI_PrintStringSmallNormal(contactList[DTMFDIGI_FindNextContact()].contactName,0,LCD_WIDTH,4);
}

void DTMFDIGI_UpdateMenu(void)
{
    uint8_t prevItem, nextItem = DTMFDIGI_menuItem+1;

    if ( DTMFDIGI_menuItem == 0)
        prevItem = DTMFDIGI_MENU_LAST-1;
    else
        prevItem = DTMFDIGI_menuItem-1;

    if ( nextItem == DTMFDIGI_MENU_LAST)
        nextItem = 0;

    UI_PrintStringSmallNormal(DTMFDIGI_MENU_ITEMS[prevItem],0, LCD_WIDTH, 1);
    UI_PrintString(DTMFDIGI_MENU_ITEMS[DTMFDIGI_menuItem],0,LCD_WIDTH,2,8);
    UI_PrintStringSmallNormal(DTMFDIGI_MENU_ITEMS[nextItem],0,LCD_WIDTH,4);    
}


void DTMFDIGI_UpdateCallScr(void)
{
    #ifdef ENABLE_DTMFDIGI_DEBUG
    DTMFDIGI_UpdateDebug();
    return;
    #endif

    char String[17];

    sprintf(String, "CALL %s", gDTMFDIGI_comm_status == COMM_STATUS_CALL_IN ? "IN" : "OUT");
    UI_PrintString(String,0,LCD_WIDTH,0,8);
    
    // Now we print the caller/callee
    uint8_t radio = ( gDTMFDIGI_comm_status == COMM_STATUS_CALL_OUT || gDTMFDIGI_comm_status == COMM_STATUS_CALLREQ_OUT ) ? gDTMFDIGI_callee : gDTMFDIGI_caller;
    sprintf(String, "%03d", radio);
    DTMFDIGI_FindContact(radio,String);
    UI_PrintString( String,0,LCD_WIDTH,2,8 );
}
#ifdef ENABLE_DTMFDIGI_DEBUG
void DTMFDIGI_UpdateDebug(void)
{
    char String[20];

    UI_PrintString("DEBUG",0,LCD_WIDTH,0,8);

    sprintf(String, "TYP: %02X CRC: %02X",gDTMFDIGI_Packet.dataType,gDTMFDIGI_Packet.checksum);
    UI_PrintStringSmallNormal(String,2,0,2);
    sprintf(String, "SND: %d REC: %d", gDTMFDIGI_Packet.senderId, gDTMFDIGI_Packet.receiverId);
    UI_PrintStringSmallNormal(String,2,0,3);
    
    gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length] = '\0';
    UI_PrintStringSmallNormal(gDTMFDIGI_RawPacket,2,0,4);

    sprintf(String,"CLL:%02X COMM:%02X", gDTMFDIGI_callStatus, gDTMFDIGI_comm_status);
    UI_PrintStringSmallNormal(String,2,0,5);

    //String = "COMM"
}
#endif

void DTMFDIGI_ForceUpdate(uint8_t screen)
{
    (void)(screen);
    //if (screen == DTMFDIGI_displayn)
        DTMFDIGI_updateDisplay = true;
}

#endif