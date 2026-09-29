/*
*/

#ifndef UI_DTMFDIGI_H
#define UI_DTMFDIGI_H
#endif

#ifdef ENABLE_DTMF_DIGITAL



#define DTMFDIGI_DISPL_MAIN     0x00    // Main menu display
#define DTMFDIGI_DISPL_CONTACTS 0x01    // Contacts list
#ifdef ENABLE_DTMFDIGI_DEBUG
#define DTMFDIGI_DISPL_DEBUG    0x02    // Display debug info
#endif
#define DTMFDIGI_DISPL_CALLSCR  0x03    // CallScreen

#define DTMFDIGI_DISPL_DTMFERR  0xFD    // DTMF disabled on selected frequency
#define DTMFDIGI_DISPL_TXERR    0xFE    // TX disabled on selected frequency
#define DTMFDIGI_DISPL_MODERR   0xFF    // Current modulation doesn't support voice 

void UI_DisplayDTMFDigi(void);
void DTMFDIGI_InitDisplay(void);
void DTMFDIGI_UpdateMenu(void);
void DTMFDIGI_ForceUpdate(uint8_t screen);
void DTMFDIGI_SwitchDisplay(uint8_t display, uint8_t return_point);
void DTMFDIGI_RestoreDisplay(void);
void DTMFDIGI_UpdateCallScr(void);

#ifdef ENABLE_DTMFDIGI_DEBUG
void DTMFDIGI_UpdateDebug(void);
#endif

extern bool         DTMFDIGI_updateDisplay;
extern uint8_t      DTMFDIGI_displayStatus;
extern uint8_t      DTMFDIGI_displayn;
extern uint8_t      DTMFDIGI_prevdisplayn;
extern uint8_t      DTMFDIGI_menuItem;
extern const char*  DTMFDIGI_MENU_ITEMS[];

enum MenuEntries
{
    DTMFDIGI_MENU_CALL = 0,
    DTMFDIGI_MENU_TEXT = 1,

#ifdef ENABLE_DTMFDIGI_DEBUG
    DTMFDIGI_MENU_DEBUG,
#endif

    DTMFDIGI_MENU_LAST
};


#endif