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

void UI_DisplayDTMFDigi(void)
{
    UI_DisplayClear(); // clear the display
    char String[32];

    sprintf(String, "IDX:%d ST:%d", gDTMFDIGI_RawPacket_length, gDTMFDIGI_request_stage);
    UI_PrintString(String, 2, 0, 0, 8);

    sprintf(String, "MY:%d PACK:%d", gDTMFDIGI_Packet.checksumt, gDTMFDIGI_Packet.checksum);
    UI_PrintString(String, 2, 0, 2, 8);
    String[0] = '\0';

    
        switch (gDTMFDIGI_Packet.error)
        {
            case PACKET_ERROR_NONE:
                sprintf(String,"OK");
                break;
            case PACKET_ERROR_CHECKSUM:
                sprintf(String,"CHECKSUM ERR");
                break;
            case PACKET_ERROR_INVALID_LENGTH:
                sprintf(String,"LEN ERR");
                break;
            case PACKET_ERROR_INVALID_TYPE:
                sprintf(String,"INV PACK ERR");
                break;
        }
        
        UI_PrintString(String, 2, 0, 4, 8);
        
        gDTMFDIGI_RawPacket[gDTMFDIGI_RawPacket_length] = '\0';
        UI_PrintStringSmallNormal(gDTMFDIGI_RawPacket,2,0,6); 


    ST7565_BlitFullScreen(); // update the display
}

#endif