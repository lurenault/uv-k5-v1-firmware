#ifdef ENABLE_APRS

#include "app/aprs.h"
#include "driver/st7565.h"
#include "ui/aprs.h"
#include "ui/helper.h"

/* Small font: 6px glyph + 1px gap → 18 chars per 128px row.
 * Layout (7 frame lines):
 *   row 1: source callsign
 *   row 2: Maidenhead (from uncompressed / compressed / Mic-E lat/lon)
 *   rows 3..6: comment / message / info text (hard-wrap)
 */
#define APRS_UI_TEXT_COLS  18u
#define APRS_UI_TEXT_ROW0  3u

void UI_DisplayAPRS(void)
{
	char          chunk[APRS_UI_TEXT_COLS + 1u];
	const char   *text;
	unsigned int  i;
	uint8_t       row;

	UI_DisplayClear();

	/* Row 1: callsign, row 2: maidenhead (centered when End=127). */
	UI_PrintStringSmallNormal(gAPRS_RxCall[0] ? gAPRS_RxCall : "-", 0, 127, 1);
	UI_PrintStringSmallNormal(gAPRS_RxGrid[0] ? gAPRS_RxGrid : "--------", 0, 127, 2);

	/* Rows 3..6: comment/text, hard-wrap; truncate past last row. */
	text = gAPRS_RxText[0] ? gAPRS_RxText : "-";
	i    = 0;
	for (row = APRS_UI_TEXT_ROW0; row < FRAME_LINES && text[i]; row++) {
		unsigned int n = 0;

		while (n < APRS_UI_TEXT_COLS && text[i])
			chunk[n++] = text[i++];
		chunk[n] = 0;
		UI_PrintStringSmallNormal(chunk, 0, 0, row);
	}

	ST7565_BlitFullScreen();
}

#endif /* ENABLE_APRS */
