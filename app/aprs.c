/* Minimal APRS: Bell 202 / HDLC / AX.25 via BK4819 FSK.
 *
 * TX/RX register recipe and bit codec are taken from
 * uv-k5-firmware-ta1js/app/aprs_minimal.c (same DP32 UV-K5).
 * Position / Mic-E decode + Maidenhead display are local parse/UI.
 * TX: New n-N digipeater (WB2OSZ-style) — not raw AX.25 echo.
 *
 * Behaviour: side-key ACTION_APRS → DISPLAY_APRS @ 144.640 FM → listen →
 * on FCS-OK frame show call / grid / comment; digipeat only when path matches.
 */

#ifdef ENABLE_APRS

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/aprs.h"
#include "app/app.h"
#ifdef ENABLE_DIGMODE
#include "app/digmode.h"
#endif
#include "audio.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "ui/ui.h"

#define APRS_RX_FREQUENCY     14464000u
#define HDLC_BUF_SIZE         240u
#define HDLC_LEAD_FLAGS       32u
#define HDLC_TAIL_FLAGS       3u
#define APRS_RX_CAPTURE_BYTES 240u
#define APRS_RX_FRAME_MAX     95u /* RX + one via insert (+7) fits */
#define APRS_DUPE_SLOTS       4u
#define APRS_DUPE_TICKS       60u /* 30 s @ 500 ms */

#define APRS_RX_IRQ_MASK (BK4819_REG_02_FSK_RX_FINISHED | \
                          BK4819_REG_02_FSK_FIFO_ALMOST_FULL | \
                          BK4819_REG_02_FSK_RX_SYNC)

char gAPRS_RxCall[APRS_RX_CALL_LEN];
char gAPRS_RxGrid[APRS_RX_GRID_LEN];
char gAPRS_RxText[APRS_RX_TEXT_LEN];

static uint8_t  gHdlcBuf[HDLC_BUF_SIZE];
static uint8_t  gRxFrame[APRS_RX_FRAME_MAX];
static uint8_t  gLastFrame[APRS_RX_FRAME_MAX];
static uint8_t  gLastLen;
static bool     gNeedRexmit;
static uint16_t gPendingDupeHash; /* hashed only after TX succeeds */
static uint8_t  gRexmitCooldown; /* 500 ms ticks; suppress self-loop */
static uint8_t  gTxCooldown;     /* 500 ms ticks after TX */
static uint16_t gDupeHash[APRS_DUPE_SLOTS];
static uint8_t  gDupeAge[APRS_DUPE_SLOTS];

static bool     gRxArmed;
static bool     gRxCapturing;
static uint8_t  gRxStuckTicks;
static uint16_t gRxCount;

static bool              gAprsRfActive;
static VFO_Info_t       *gAprsSnapVfo;
static uint32_t          gAprsSnapRxFreq;
static uint32_t          gAprsSnapTxFreq;
static ModulationMode_t  gAprsSnapMod;
static DCS_CodeType_t    gAprsSnapRxCodeType;
static DCS_CodeType_t    gAprsSnapTxCodeType;
static uint8_t           gAprsSnapCompander;
static uint8_t           gAprsSnapBandwidth;

/* ---- CRC / AX.25 (ta1js) ------------------------------------------------ */

static uint16_t APRS_CalculateCRC(const uint8_t *data, uint16_t length)
{
	uint16_t crc = 0xFFFF;
	for (uint16_t i = 0; i < length; i++) {
		crc ^= (uint16_t)data[i];
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc >>= 1;
		}
	}
	return crc;
}

static uint16_t AX25_CalculateFCS(const uint8_t *data, uint16_t length)
{
	return (uint16_t)(~APRS_CalculateCRC(data, length));
}

/* ---- Digipeater helpers (New n-N) --------------------------------------- */

static void APRS_PutCall(uint8_t *dst, const char *call, uint8_t ssid, bool hbit, bool end)
{
	for (uint8_t i = 0; i < 6; i++) {
		const char c = call[i] ? call[i] : ' ';
		dst[i] = (uint8_t)((uint8_t)c << 1);
	}
	dst[6] = (uint8_t)(0x60u | ((ssid & 0x0Fu) << 1));
	if (hbit)
		dst[6] |= 0x80u;
	if (end)
		dst[6] |= 0x01u;
}

static void APRS_PutWide(uint8_t *dst, char nch, uint8_t N, bool hbit, bool end)
{
	static const char w[] = "WIDE";
	for (uint8_t i = 0; i < 4; i++)
		dst[i] = (uint8_t)((uint8_t)w[i] << 1);
	dst[4] = (uint8_t)((uint8_t)nch << 1);
	dst[5] = (uint8_t)(' ' << 1);
	dst[6] = (uint8_t)(0x60u | ((N & 0x0Fu) << 1));
	if (hbit)
		dst[6] |= 0x80u;
	if (end)
		dst[6] |= 0x01u;
}

static bool APRS_AddrEqCall(const uint8_t *addr, const char *call, uint8_t ssid)
{
	for (uint8_t i = 0; i < 6; i++) {
		const char c = (char)(addr[i] >> 1);
		const char r = call[i] ? call[i] : ' ';
		if (c != r)
			return false;
	}
	return ((addr[6] >> 1) & 0x0Fu) == (ssid & 0x0Fu);
}

static bool APRS_AddrIsWide(const uint8_t *addr, char nch, uint8_t *N)
{
	if ((char)(addr[0] >> 1) != 'W' || (char)(addr[1] >> 1) != 'I' ||
	    (char)(addr[2] >> 1) != 'D' || (char)(addr[3] >> 1) != 'E' ||
	    (char)(addr[4] >> 1) != nch || (char)(addr[5] >> 1) != ' ')
		return false;
	*N = (uint8_t)((addr[6] >> 1) & 0x0Fu);
	return *N >= 1u;
}

static uint16_t APRS_DupeHash(const uint8_t *frame, uint16_t len)
{
	uint16_t a = 0;
	uint16_t h;

	while (a + 7u <= len - 2u && (frame[a + 6] & 1u) == 0)
		a += 7;
	if (a + 7u > len - 2u)
		return 0;
	a += 7; /* past last address */
	/* dest(7)+src(7)+info — skip vias */
	h = APRS_CalculateCRC(frame, 14);
	if (a + 2u < len - 2u)
		h = (uint16_t)(h ^ APRS_CalculateCRC(&frame[a + 2u], (uint16_t)(len - 2u - (a + 2u))));
	return h ? h : 1u;
}

static bool APRS_DupeHit(uint16_t hash)
{
	for (uint8_t i = 0; i < APRS_DUPE_SLOTS; i++) {
		if (gDupeAge[i] && gDupeHash[i] == hash)
			return true;
	}
	return false;
}

static void APRS_DupeAdd(uint16_t hash)
{
	uint8_t slot = 0;
	uint8_t age  = gDupeAge[0];

	for (uint8_t i = 0; i < APRS_DUPE_SLOTS; i++) {
		if (gDupeAge[i] == 0) {
			slot = i;
			break;
		}
		if (gDupeAge[i] < age) {
			age  = gDupeAge[i];
			slot = i;
		}
	}
	gDupeHash[slot] = hash;
	gDupeAge[slot]  = APRS_DUPE_TICKS;
}

/* Build digipeated frame into out. Returns length incl FCS, or 0. */
static uint8_t APRS_BuildDigi(const uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_max)
{
	uint16_t addr_end = 0;
	uint16_t cand     = 0xFFFF;
	uint16_t via;
	uint8_t  N        = 0;
	uint16_t out_len  = in_len;

	if (in_len < 20u || in_len > out_max || gAPRS_DigiCall[0] == 0)
		return 0;
	if ((gAPRS_DigiFlags & APRS_DIGI_FLAG_ON) == 0)
		return 0;

	for (;;) {
		if (addr_end + 7u > in_len - 2u)
			return 0;
		if (in[addr_end + 6] & 1u) {
			addr_end += 7;
			break;
		}
		addr_end += 7;
	}
	if (addr_end < 14u || addr_end + 2u > in_len - 2u)
		return 0;
	if (in[addr_end] != 0x03 && in[addr_end] != 0x13)
		return 0;
	if (in[addr_end + 1] != 0xF0)
		return 0;

	/* Do not digipeat our own packets (call+SSID). */
	if (APRS_AddrEqCall(&in[7], gAPRS_DigiCall, gAPRS_DigiSSID))
		return 0;

	for (via = 14; via + 7u <= addr_end; via += 7) {
		const bool h = (in[via + 6] & 0x80u) != 0;
		if (h) {
			if (APRS_AddrEqCall(&in[via], gAPRS_DigiCall, gAPRS_DigiSSID))
				return 0;
		} else if (cand == 0xFFFF) {
			cand = via;
		}
	}
	if (cand == 0xFFFF)
		return 0;

	if (APRS_AddrEqCall(&in[cand], gAPRS_DigiCall, gAPRS_DigiSSID)) {
		memcpy(out, in, in_len);
		out[cand + 6] |= 0x80u;
	} else if ((gAPRS_DigiFlags & APRS_DIGI_FLAG_WIDE1) &&
		   APRS_AddrIsWide(&in[cand], '1', &N) && N == 1u) {
		memcpy(out, in, in_len);
		APRS_PutCall(&out[cand], gAPRS_DigiCall, gAPRS_DigiSSID, true,
			     (in[cand + 6] & 1u) != 0);
	} else if ((gAPRS_DigiFlags & APRS_DIGI_FLAG_WIDE2) &&
		   APRS_AddrIsWide(&in[cand], '2', &N)) {
		if (N == 1u) {
			memcpy(out, in, in_len);
			APRS_PutCall(&out[cand], gAPRS_DigiCall, gAPRS_DigiSSID, true,
				     (in[cand + 6] & 1u) != 0);
		} else {
			/* AX.25 max 8 addresses — refuse insert when full */
			if (addr_end >= 56u || in_len + 7u > out_max)
				return 0;
			memcpy(out, in, cand);
			APRS_PutCall(&out[cand], gAPRS_DigiCall, gAPRS_DigiSSID, true, false);
			APRS_PutWide(&out[cand + 7], '2', (uint8_t)(N - 1u), false,
				     (in[cand + 6] & 1u) != 0);
			memcpy(&out[cand + 14], &in[cand + 7], in_len - (cand + 7u));
			out_len = (uint16_t)(in_len + 7u);
		}
	} else {
		return 0;
	}

	{
		const uint16_t fcs = AX25_CalculateFCS(out, (uint16_t)(out_len - 2u));
		out[out_len - 2u] = (uint8_t)fcs;
		out[out_len - 1u] = (uint8_t)(fcs >> 8);
	}
	return (uint8_t)out_len;
}

static void APRS_ConsiderDigipeat(const uint8_t *frame, uint16_t len)
{
	uint16_t h;
	uint8_t  n;

	if (gRexmitCooldown || len < 20u || len > sizeof(gLastFrame))
		return;

	h = APRS_DupeHash(frame, len);
	if (APRS_DupeHit(h))
		return; /* dupe: still shown, no TX */

	n = APRS_BuildDigi(frame, len, gLastFrame, sizeof(gLastFrame));
	if (n == 0)
		return;
	gPendingDupeHash = h;
	gLastLen         = n;
	gNeedRexmit      = true;
}

/* ---- HDLC bitstream writer (ta1js) -------------------------------------- */

typedef struct {
	uint8_t *buf;
	uint16_t bits;
	uint8_t  ones;
	uint8_t  level;
} hdlc_writer_t;

static void HDLC_PutBit(hdlc_writer_t *w, bool bit)
{
	if (w->bits >= HDLC_BUF_SIZE * 8u)
		return;
	if (!bit)
		w->level ^= 1;
	if (w->level)
		w->buf[w->bits >> 3] |= (uint8_t)(0x80u >> (w->bits & 7u));
	w->bits++;
}

static void HDLC_PutByte(hdlc_writer_t *w, uint8_t b, bool stuff)
{
	for (uint8_t i = 0; i < 8; i++) {
		const bool bit = (b >> i) & 1u;
		HDLC_PutBit(w, bit);
		if (stuff && bit) {
			if (++w->ones == 5) {
				HDLC_PutBit(w, false);
				w->ones = 0;
			}
		} else {
			w->ones = 0;
		}
	}
}

/* ---- Bell 202 TX via BK4819 FSK (ta1js APRS_TransmitBell202) ------------ */

static bool APRS_TransmitBell202(const uint8_t *frame, uint16_t frame_len)
{
	uint16_t i;

	memset(gHdlcBuf, 0, sizeof(gHdlcBuf));
	hdlc_writer_t w = { gHdlcBuf, 0, 0, 1 };

	for (i = 0; i < HDLC_LEAD_FLAGS; i++)
		HDLC_PutByte(&w, 0x7E, false);
	for (i = 0; i < frame_len; i++)
		HDLC_PutByte(&w, frame[i], true);
	for (i = 0; i < HDLC_TAIL_FLAGS; i++)
		HDLC_PutByte(&w, 0x7E, false);

	uint16_t nbytes = (uint16_t)((w.bits + 7u) / 8u);
	if (nbytes & 1u)
		nbytes++;

	RADIO_PrepareTX();
	if (gCurrentFunction != FUNCTION_TRANSMIT)
		return false;

	BK4819_SetAF(BK4819_AF_MUTE);

	const uint16_t css_val  = BK4819_ReadRegister(BK4819_REG_51);
	const uint16_t dev_val  = BK4819_ReadRegister(BK4819_REG_40);
	const uint16_t filt_val = BK4819_ReadRegister(BK4819_REG_2B);

	BK4819_WriteRegister(BK4819_REG_51, 0);
	BK4819_WriteRegister(BK4819_REG_40, (uint16_t)((dev_val & 0xF000u) | 1200u));
	BK4819_WriteRegister(BK4819_REG_2B, (uint16_t)((1u << 2) | (1u << 0)));

	BK4819_WriteRegister(BK4819_REG_70, (uint16_t)((1u << 15) | (1u << 7) | (96u << 0)));
	BK4819_WriteRegister(BK4819_REG_71, 22714);
	BK4819_WriteRegister(BK4819_REG_72, 12389);

	BK4819_WriteRegister(BK4819_REG_58,
		(uint16_t)((1u << 13) | (7u << 10) | (3u << 8) | (3u << 6) | (1u << 1) | (1u << 0)));

	BK4819_WriteRegister(BK4819_REG_5A, 0xAAAA);
	BK4819_WriteRegister(BK4819_REG_5B, 0xAAAA);
	BK4819_WriteRegister(BK4819_REG_5C, 0xAA30);
	BK4819_WriteRegister(BK4819_REG_5D, (uint16_t)((nbytes - 1u) << 8));

	BK4819_WriteRegister(BK4819_REG_59, 0x8068);
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);

	for (i = 0; i < nbytes; i += 2)
		BK4819_WriteRegister(BK4819_REG_5F,
			(uint16_t)((gHdlcBuf[i + 1] << 8) | gHdlcBuf[i]));

	SYSTEM_DelayMs(20);
	BK4819_WriteRegister(BK4819_REG_59, 0x0868);
	SYSTEM_DelayMs(((10u + (uint32_t)nbytes) * 20u) / 3u + 100u);

	BK4819_WriteRegister(BK4819_REG_59, 0x0068);
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_58, 0x0000);
	BK4819_WriteRegister(BK4819_REG_40, dev_val);
	BK4819_WriteRegister(BK4819_REG_2B, filt_val);
	BK4819_WriteRegister(BK4819_REG_51, css_val);

	APP_EndTransmission();
	FUNCTION_Select(FUNCTION_FOREGROUND);

	gTxCooldown     = 2; /* 1 s */
	gRexmitCooldown = 6; /* 3 s ignore own RF loop */
	APRS_DupeAdd(gPendingDupeHash);
	return true;
}

/* ---- Temp RF @ 144.640 (enter/exit) ------------------------------------ */

static void APRS_ClearDisplayFields(void)
{
	gAPRS_RxCall[0] = '-';
	gAPRS_RxCall[1] = 0;
	memcpy(gAPRS_RxGrid, "--------", 9);
	gAPRS_RxText[0] = '-';
	gAPRS_RxText[1] = 0;
}

static void APRS_CopyText(char *dst, uint8_t dst_len, const uint8_t *src, uint16_t src_len)
{
	uint8_t o = 0;

	if (dst_len == 0)
		return;
	for (uint16_t i = 0; i < src_len && o + 1u < dst_len; i++) {
		const char c = (char)src[i];
		if (c < 32 || c >= 127)
			continue;
		dst[o++] = c;
	}
	while (o > 0 && dst[o - 1u] == ' ')
		o--;
	if (o == 0)
		dst[0] = '-';
	else
		dst[o] = 0;
}

/* 8-character Maidenhead (fields/squares/subsquares/ext). South/west negative. */
static void APRS_ToMaidenhead8(int32_t lat_udeg, int32_t lon_udeg, char out[9])
{
	if (lat_udeg >  89999999) lat_udeg =  89999999;
	if (lat_udeg < -90000000) lat_udeg = -90000000;
	if (lon_udeg >  179999999) lon_udeg =  179999999;
	if (lon_udeg < -180000000) lon_udeg = -180000000;

	{
		uint32_t lon = (uint32_t)(lon_udeg + 180000000);
		uint32_t lat = (uint32_t)(lat_udeg +  90000000);

		out[0] = (char)('A' + lon / 20000000u);
		out[1] = (char)('A' + lat / 10000000u);
		lon %= 20000000u;
		lat %= 10000000u;

		out[2] = (char)('0' + lon / 2000000u);
		out[3] = (char)('0' + lat / 1000000u);
		lon %= 2000000u;
		lat %= 1000000u;

		{
			const uint32_t lon_ss = (lon * 24u) / 2000000u;
			const uint32_t lat_ss = (lat * 24u) / 1000000u;
			out[4] = (char)('a' + (lon_ss > 23u ? 23u : lon_ss));
			out[5] = (char)('a' + (lat_ss > 23u ? 23u : lat_ss));
			out[6] = (char)('0' + ((lon * 24u) % 2000000u) * 10u / 2000000u);
			out[7] = (char)('0' + ((lat * 24u) % 1000000u) * 10u / 1000000u);
		}
		out[8] = 0;
	}
}

static uint8_t MIN100_TO_MICRO(uint32_t deg, uint32_t min100, int32_t *out)
{
	if (deg > 180u || min100 >= 6000u)
		return 0;
	*out = (int32_t)(deg * 1000000u + (min100 * 500u) / 3u);
	return 1;
}

static uint8_t APRS_ParseUncompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon)
{
	const uint8_t *q = p;
	uint32_t       latd, latm, lond, lonm;

	if (len < 19)
		return 0;
#define DIGIT(c) (((c) == ' ') ? 0u : (uint32_t)((c) - '0'))
#define ISDIG(c) (((c) >= '0' && (c) <= '9') || (c) == ' ')
	if (!ISDIG(q[0]) || !ISDIG(q[1]) || !ISDIG(q[2]) || !ISDIG(q[3]) ||
	    q[4] != '.' || !ISDIG(q[5]) || !ISDIG(q[6]))
		return 0;
	latd = DIGIT(q[0]) * 10u + DIGIT(q[1]);
	latm = DIGIT(q[2]) * 1000u + DIGIT(q[3]) * 100u + DIGIT(q[5]) * 10u + DIGIT(q[6]);
	if (!MIN100_TO_MICRO(latd, latm, lat))
		return 0;
	if (q[7] == 'S')
		*lat = -*lat;
	else if (q[7] != 'N')
		return 0;
	q += 9;
	if (!ISDIG(q[0]) || !ISDIG(q[1]) || !ISDIG(q[2]) || !ISDIG(q[3]) ||
	    !ISDIG(q[4]) || q[5] != '.' || !ISDIG(q[6]) || !ISDIG(q[7]))
		return 0;
	lond = DIGIT(q[0]) * 100u + DIGIT(q[1]) * 10u + DIGIT(q[2]);
	lonm = DIGIT(q[3]) * 1000u + DIGIT(q[4]) * 100u + DIGIT(q[6]) * 10u + DIGIT(q[7]);
	if (!MIN100_TO_MICRO(lond, lonm, lon))
		return 0;
	if (q[8] == 'W')
		*lon = -*lon;
	else if (q[8] != 'E')
		return 0;
#undef DIGIT
#undef ISDIG
	return 1;
}

static uint8_t APRS_ParseMicE(const uint8_t *frame, const uint8_t *info, uint16_t ilen,
			      int32_t *lat, int32_t *lon)
{
	const uint8_t t = info[0];
	uint8_t       dig[6], bit[6];
	uint32_t      latd, latm;
	int32_t       d, m, h;

	if (t != 0x60 && t != 0x27 && t != 0x1C && t != 0x1D)
		return 0;
	if (ilen < 9)
		return 0;

	for (uint8_t i = 0; i < 6; i++) {
		const char c = (char)(frame[i] >> 1);
		if (c >= '0' && c <= '9')      { dig[i] = (uint8_t)(c - '0'); bit[i] = 0; }
		else if (c >= 'A' && c <= 'J') { dig[i] = (uint8_t)(c - 'A'); bit[i] = 1; }
		else if (c >= 'P' && c <= 'Y') { dig[i] = (uint8_t)(c - 'P'); bit[i] = 1; }
		else if (c == 'L')             { dig[i] = 0; bit[i] = 0; }
		else if (c == 'K' || c == 'Z') { dig[i] = 0; bit[i] = 1; }
		else return 0;
	}
	latd = (uint32_t)dig[0] * 10u + dig[1];
	latm = (uint32_t)dig[2] * 1000u + (uint32_t)dig[3] * 100u
	     + (uint32_t)dig[4] * 10u + dig[5];
	if (latd > 90u || !MIN100_TO_MICRO(latd, latm, lat))
		return 0;
	if (!bit[3])
		*lat = -*lat;

	/* Lon degrees: APRS 1.01 order (−28, optional +100, then 180/190 fix). */
	d = (int32_t)info[1] - 28;
	m = (int32_t)info[2] - 28;
	h = (int32_t)info[3] - 28;
	if (d < 0 || m < 0 || h < 0 || h > 99)
		return 0;
	if (bit[4])
		d += 100;
	if (d >= 180 && d <= 189)
		d -= 80;
	else if (d >= 190 && d <= 199)
		d -= 190;
	if (m >= 60)
		m -= 60;
	if (d > 179 || m > 59)
		return 0;
	if (!MIN100_TO_MICRO((uint32_t)d, (uint32_t)(m * 100 + h), lon))
		return 0;
	if (bit[5])
		*lon = -*lon;
	return 1;
}

static uint8_t APRS_ParseCompressed(const uint8_t *p, uint16_t len, int32_t *lat, int32_t *lon)
{
	uint32_t y = 0, x = 0;

	if (len < 10)
		return 0;
	for (uint8_t i = 1; i <= 8; i++)
		if (p[i] < '!' || p[i] > '{')
			return 0;
	for (uint8_t i = 1; i <= 4; i++)
		y = y * 91u + (uint32_t)(p[i] - 33);
	for (uint8_t i = 5; i <= 8; i++)
		x = x * 91u + (uint32_t)(p[i] - 33);
	{
		const uint32_t dd = y / 380926u, rr = y % 380926u;
		if (dd > 180u)
			return 0;
		*lat = 90000000 - (int32_t)(dd * 1000000u + (rr * 21u) / 8u);
	}
	{
		const uint32_t dd = x / 190463u, rr = x % 190463u;
		if (dd > 360u)
			return 0;
		*lon = -180000000 + (int32_t)(dd * 1000000u + (rr * 21u) / 4u);
	}
	return 1;
}

static void APRS_ApplyFixedRf(void)
{
	if (gAprsRfActive || gRxVfo == NULL)
		return;

	gAprsSnapVfo        = gRxVfo;
	gAprsSnapRxFreq     = gRxVfo->freq_config_RX.Frequency;
	gAprsSnapTxFreq     = gRxVfo->freq_config_TX.Frequency;
	gAprsSnapMod        = gRxVfo->Modulation;
	gAprsSnapRxCodeType = gRxVfo->freq_config_RX.CodeType;
	gAprsSnapTxCodeType = gRxVfo->freq_config_TX.CodeType;
	gAprsSnapCompander  = gRxVfo->Compander;
	gAprsSnapBandwidth  = gRxVfo->CHANNEL_BANDWIDTH;

	gRxVfo->freq_config_RX.Frequency = APRS_RX_FREQUENCY;
	gRxVfo->freq_config_TX.Frequency = APRS_RX_FREQUENCY;
	gRxVfo->Modulation               = MODULATION_FM;
	gRxVfo->freq_config_RX.CodeType  = CODE_TYPE_OFF;
	gRxVfo->freq_config_TX.CodeType  = CODE_TYPE_OFF;
	gRxVfo->Compander                = 0;
	gRxVfo->CHANNEL_BANDWIDTH        = BANDWIDTH_WIDE;
	gAprsRfActive = true;

	RADIO_SetupRegisters(true);
}

static void APRS_RestoreRf(void)
{
	if (!gAprsRfActive || gAprsSnapVfo == NULL)
		return;

	gAprsSnapVfo->freq_config_RX.Frequency = gAprsSnapRxFreq;
	gAprsSnapVfo->freq_config_TX.Frequency = gAprsSnapTxFreq;
	gAprsSnapVfo->Modulation               = gAprsSnapMod;
	gAprsSnapVfo->freq_config_RX.CodeType  = gAprsSnapRxCodeType;
	gAprsSnapVfo->freq_config_TX.CodeType  = gAprsSnapTxCodeType;
	gAprsSnapVfo->Compander                = gAprsSnapCompander;
	gAprsSnapVfo->CHANNEL_BANDWIDTH        = gAprsSnapBandwidth;
	gAprsRfActive = false;
	gAprsSnapVfo  = NULL;

	RADIO_SetupRegisters(true);
}

/* ---- RX arm / drain / decode (ta1js) ------------------------------------ */

static void APRS_RxArm(void)
{
	BK4819_WriteRegister(BK4819_REG_70, (uint16_t)((1u << 15) | (1u << 7) | (96u << 0)));
	BK4819_WriteRegister(BK4819_REG_71, 22714);
	BK4819_WriteRegister(BK4819_REG_72, 12389);

	BK4819_WriteRegister(BK4819_REG_58,
		(uint16_t)((1u << 13) | (7u << 10) | (3u << 8) | (3u << 6) | (1u << 1) | (1u << 0)));

	BK4819_WriteRegister(BK4819_REG_5A, 0xFEFE);
	BK4819_WriteRegister(BK4819_REG_5C, 0xAA30);
	BK4819_WriteRegister(BK4819_REG_5D, (uint16_t)((APRS_RX_CAPTURE_BYTES - 1u) << 8));
	BK4819_WriteRegister(BK4819_REG_5E, (uint16_t)((64u << 3) | (4u << 0)));

	const uint16_t mask = BK4819_ReadRegister(BK4819_REG_3F);
	BK4819_WriteRegister(BK4819_REG_3F, (uint16_t)(mask | APRS_RX_IRQ_MASK));

	BK4819_WriteRegister(BK4819_REG_59, 0x4000);
	BK4819_WriteRegister(BK4819_REG_59, 0x1000);

	gRxCount      = 0;
	gRxCapturing  = false;
	gRxStuckTicks = 0;
	gRxArmed      = true;
}

static void APRS_RxDrainFifo(uint8_t words)
{
	while (words-- > 0) {
		const uint16_t w = BK4819_ReadRegister(BK4819_REG_5F);
		if (gRxCount < HDLC_BUF_SIZE)
			gHdlcBuf[gRxCount++] = (uint8_t)w;
		if (gRxCount < HDLC_BUF_SIZE)
			gHdlcBuf[gRxCount++] = (uint8_t)(w >> 8);
	}
}

void APRS_StopListening(void)
{
	if (!gRxArmed && !gAprsRfActive)
		return;

	if (gRxArmed) {
		BK4819_WriteRegister(BK4819_REG_59, 0x0000);
		BK4819_WriteRegister(BK4819_REG_58, 0x0000);
		BK4819_WriteRegister(BK4819_REG_70, 0x0000);
		gRxArmed     = false;
		gRxCapturing = false;
	}

	APRS_RestoreRf();
}

static void APRS_ShowFrame(const uint8_t *frame, uint16_t len)
{
	uint16_t       a = 6;
	const uint8_t *ip;
	uint16_t       ilen;
	int32_t        lat = 0, lon = 0;
	uint8_t        have = 0;
	const uint8_t *comment = NULL;
	uint16_t       clen    = 0;

	while (a + 7 < len && (frame[a] & 1u) == 0)
		a += 7;
	{
		const uint16_t info = (uint16_t)(a + 3);
		if (a < 13 || info >= len - 2u)
			return;
		ip   = &frame[info];
		ilen = (uint16_t)(len - 2u - info);
	}

	/* Source callsign (second address field). */
	{
		uint8_t o = 0;
		for (uint8_t i = 7; i < 13; i++) {
			const char c = (char)(frame[i] >> 1);
			if (c > ' ' && o + 1u < sizeof(gAPRS_RxCall))
				gAPRS_RxCall[o++] = c;
		}
		{
			const uint8_t ssid = (uint8_t)((frame[13] >> 1) & 0x0F);
			if (ssid > 0 && o + 2u < sizeof(gAPRS_RxCall)) {
				gAPRS_RxCall[o++] = '-';
				if (ssid >= 10 && o + 1u < sizeof(gAPRS_RxCall))
					gAPRS_RxCall[o++] = '1';
				if (o + 1u < sizeof(gAPRS_RxCall))
					gAPRS_RxCall[o++] = (char)('0' + (ssid % 10));
			}
		}
		if (o == 0)
			gAPRS_RxCall[0] = '-';
		else
			gAPRS_RxCall[o] = 0;
	}

	memcpy(gAPRS_RxGrid, "--------", 9);
	gAPRS_RxText[0] = '-';
	gAPRS_RxText[1] = 0;

	if (ilen >= 11 && ip[0] == ':' && ip[10] == ':') {
		/* APRS message — text after second colon. */
		APRS_CopyText(gAPRS_RxText, sizeof(gAPRS_RxText), &ip[11], (uint16_t)(ilen - 11));
	} else if (ilen >= 2) {
		const uint8_t t = ip[0];
		if (t == '!' || t == '=' || t == '@' || t == '/') {
			const uint8_t *p2 = ip + 1;
			uint16_t       l2 = (uint16_t)(ilen - 1);
			if ((t == '@' || t == '/') && l2 > 7) {
				p2 += 7;
				l2 -= 7;
			}
			if (l2 >= 1 && ((p2[0] >= '0' && p2[0] <= '9') || p2[0] == ' ')) {
				have = APRS_ParseUncompressed(p2, l2, &lat, &lon);
				if (have && l2 > 19) {
					comment = p2 + 19;
					clen    = (uint16_t)(l2 - 19);
				}
			} else {
				have = APRS_ParseCompressed(p2, l2, &lat, &lon);
				if (have && l2 > 10) {
					/* Skip symbol; optional csT (3 bytes) if present. */
					uint16_t off = 10;
					if (l2 >= 13 && p2[10] >= '!' && p2[10] <= '{')
						off = 13;
					if (l2 > off) {
						comment = p2 + off;
						clen    = (uint16_t)(l2 - off);
					}
				}
			}
		} else if (APRS_ParseMicE(frame, ip, ilen, &lat, &lon)) {
			have = 1;
			if (ilen > 9) {
				comment = ip + 9;
				clen    = (uint16_t)(ilen - 9);
			}
		} else if (ilen > 0) {
			/* Unknown info type — show printable info as text. */
			APRS_CopyText(gAPRS_RxText, sizeof(gAPRS_RxText), ip, ilen);
		}
	}

	if (have) {
		APRS_ToMaidenhead8(lat, lon, gAPRS_RxGrid);
		if (comment != NULL && clen > 0)
			APRS_CopyText(gAPRS_RxText, sizeof(gAPRS_RxText), comment, clen);
	}

	gUpdateDisplay = true;
}

static bool APRS_DecodeCapture(void)
{
	uint8_t  prev = 0, ones = 0;
	uint16_t nbits = 0xFFFF;

	memset(gRxFrame, 0, sizeof(gRxFrame));

	for (uint32_t i = 0; i < (uint32_t)gRxCount * 8u; i++) {
		const uint8_t level = (uint8_t)((gHdlcBuf[i >> 3] >> (7u - (i & 7u))) & 1u);
		const uint8_t bit   = (level == prev) ? 1u : 0u;
		prev = level;

		if (bit) {
			if (ones < 7)
				ones++;
			if (ones >= 7) {
				nbits = 0xFFFF;
				continue;
			}
			if (nbits != 0xFFFF) {
				if (nbits < sizeof(gRxFrame) * 8u)
					gRxFrame[nbits >> 3] |= (uint8_t)(1u << (nbits & 7u));
				nbits++;
			}
			continue;
		}

		if (ones == 5) {
			ones = 0;
			continue;
		}
		if (ones == 6) {
			if (nbits != 0xFFFF && nbits >= 7u) {
				const uint16_t fb = (uint16_t)(nbits - 7u);
				if ((fb & 7u) == 0 && fb >= 17u * 8u && (fb >> 3) <= sizeof(gRxFrame)) {
					const uint16_t len = (uint16_t)(fb >> 3);
					const uint16_t fcs = AX25_CalculateFCS(gRxFrame, (uint16_t)(len - 2u));
					if (fcs == (uint16_t)(gRxFrame[len - 2] | (gRxFrame[len - 1] << 8))) {
						APRS_ShowFrame(gRxFrame, len);
						APRS_ConsiderDigipeat(gRxFrame, len);
						return true;
					}
				}
			}
			memset(gRxFrame, 0, sizeof(gRxFrame));
			nbits = 0;
			ones  = 0;
			continue;
		}
		if (nbits != 0xFFFF) {
			if (nbits < sizeof(gRxFrame) * 8u)
				nbits++;
			else
				nbits = 0xFFFF;
		}
		ones = 0;
	}
	return false;
}

void APRS_HandleRxInterrupts(uint16_t interrupt_bits)
{
	if (!gRxArmed || gScreenToDisplay != DISPLAY_APRS)
		return;

	if (interrupt_bits & BK4819_REG_02_FSK_RX_SYNC) {
		gRxCapturing  = true;
		gRxStuckTicks = 0;
		gRxCount      = 0;
	}

	if (interrupt_bits & BK4819_REG_02_FSK_FIFO_ALMOST_FULL) {
		gRxStuckTicks = 0;
		APRS_RxDrainFifo(4);
	}

	if (interrupt_bits & BK4819_REG_02_FSK_RX_FINISHED) {
		APRS_RxDrainFifo(8);
		APRS_DecodeCapture();
		APRS_RxArm();
	}
}

void APRS_Task(void)
{
	if (gRexmitCooldown)
		gRexmitCooldown--;
	if (gTxCooldown)
		gTxCooldown--;
	for (uint8_t i = 0; i < APRS_DUPE_SLOTS; i++) {
		if (gDupeAge[i])
			gDupeAge[i]--;
	}

	if (gScreenToDisplay != DISPLAY_APRS) {
		if (gRxArmed || gAprsRfActive)
			APRS_StopListening();
		gNeedRexmit = false;
		return;
	}

	if (!gAprsRfActive)
		APRS_ApplyFixedRf();

	if (gNeedRexmit && gLastLen > 0 && gTxCooldown == 0 &&
	    gCurrentFunction != FUNCTION_TRANSMIT) {
		gNeedRexmit = false;
		/* Disarm modem only — keep temp RF for retransmit + re-arm. */
		if (gRxArmed) {
			BK4819_WriteRegister(BK4819_REG_59, 0x0000);
			BK4819_WriteRegister(BK4819_REG_58, 0x0000);
			BK4819_WriteRegister(BK4819_REG_70, 0x0000);
			gRxArmed     = false;
			gRxCapturing = false;
		}
		if (!APRS_TransmitBell202(gLastFrame, gLastLen))
			gNeedRexmit = true; /* channel busy — retry later */
		return;
	}

	if (gCurrentFunction == FUNCTION_TRANSMIT || gTxCooldown)
		return;

	if (!gRxArmed) {
		APRS_RxArm();
	} else if (gRxCapturing && ++gRxStuckTicks > 2) {
		APRS_RxDrainFifo(8);
		APRS_DecodeCapture();
		APRS_RxArm();
	}

	if (gRxArmed) {
		const uint16_t mask = BK4819_ReadRegister(BK4819_REG_3F);
		if ((mask & APRS_RX_IRQ_MASK) != APRS_RX_IRQ_MASK)
			BK4819_WriteRegister(BK4819_REG_3F, (uint16_t)(mask | APRS_RX_IRQ_MASK));
		AUDIO_AudioPathOff();
		gEnableSpeaker = false;
	}
}

void ACTION_APRS(void)
{
#ifdef ENABLE_DIGMODE
	if (gScreenToDisplay == DISPLAY_DIGMODE)
		DIGMODE_Exit();
#endif

	if (gScreenToDisplay == DISPLAY_APRS) {
		APRS_StopListening();
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		gRequestDisplayScreen = DISPLAY_MAIN;
		return;
	}

	APRS_ClearDisplayFields();
	gNeedRexmit  = false;
	gLastLen     = 0;
	GUI_SelectNextDisplay(DISPLAY_APRS);
	APRS_ApplyFixedRf();
	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
}

void APRS_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyPressed || bKeyHeld)
		return;

	if (Key == KEY_EXIT) {
		APRS_StopListening();
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		gRequestDisplayScreen = DISPLAY_MAIN;
		return;
	}

	gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

#endif /* ENABLE_APRS */
