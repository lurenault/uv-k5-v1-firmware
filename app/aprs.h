/* Minimal APRS TX/RX for UV-K5 (k5-v6).
 * Modem/HDLC/AX.25 path follows uv-k5-firmware-ta1js app/aprs_minimal.c.
 * Position/Mic-E decode + Maidenhead UI fields are local parse/display.
 * TX echo remains raw AX.25 retransmit.
 */

#ifndef APP_APRS_H
#define APP_APRS_H

#ifdef ENABLE_APRS

#include <stdbool.h>
#include <stdint.h>

#include "driver/keyboard.h"

#define APRS_RX_CALL_LEN  10u
#define APRS_RX_GRID_LEN  9u
/* Small font: 18 cols × 4 text rows (LCD lines 3..6) + NUL. */
#define APRS_RX_TEXT_LEN  73u

/* Latest successful decode (sticky until next packet). */
extern char gAPRS_RxCall[APRS_RX_CALL_LEN];
extern char gAPRS_RxGrid[APRS_RX_GRID_LEN];
extern char gAPRS_RxText[APRS_RX_TEXT_LEN];

void ACTION_APRS(void);
void APRS_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void APRS_Task(void);
void APRS_HandleRxInterrupts(uint16_t irq_bits);
void APRS_StopListening(void);

#endif /* ENABLE_APRS */

#endif /* APP_APRS_H */
