/*
 * ExtIO wrapper
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define LIBRTL_EXPORTS 1

#define ALWAYS_PCMU8	0
#define ALWAYS_PCM16	0

/* 0 == just filter (sum) without decimation
* 1 == do full decimation
*/
#define FULL_DECIMATION		1

#define WITH_AGCS		0

#define SETTINGS_IDENTIFIER "RTL_TCP_2021.1-1"


#include <stdint.h>
#include <ActiveSocket.h>

#include <Windows.h>
#include <WindowsX.h>
#include <commctrl.h>
#include <process.h>
#include <tchar.h>

#include <new>
#include <stdio.h>

#include "resource.h"
#include "ExtIO_RTL.h"

#include "LC_ExtIO_Types.h"
#include "rtl_tcp.h"      // enum values for the commands

#include "config_file.h"

#ifdef _MSC_VER
	#pragma warning(disable : 4996)
	#define snprintf  _snprintf
#endif

#if ALWAYS_PCMU8
#define MAX_DECIMATIONS		1
#else
#define MAX_DECIMATIONS		8
#endif


extern "C" int  LIBRTL_API __stdcall SetAttenuator(int atten_idx);
extern "C" int  LIBRTL_API __stdcall ExtIoSetMGC(int mgc_idx);



static int buffer_sizes[] = { //in kBytes
	1, 2, 4, 8, 16, 32, 64, 128, 256
};


#define MAX_PPM	1000
#define MIN_PPM	-1000

// 225001 - 300000 Hz, 900001 - 3200000 Hz
#define MAXRATE		3200000
#define MINRATE		900001


#if ALWAYS_PCMU8
extHWtypeT extHWtype = exthwUSBdataU8;	/* ExtIO type 8-bit samples */
#else
extHWtypeT extHWtype = exthwUSBdata16;  /* default ExtIO type 16-bit samples */
#endif


const char * TunerName[] = { "None", "E4000", "FC0012", "FC0013", "FC2580", "R820T/2", "R828D" };
static const int n_tuners = sizeof(TunerName) / sizeof(TunerName[0]);


const int e4k_gains[] =
{ -10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420 };

const int fc12_gains[] = { -99, -40, 71, 179, 192 };

const int fc13_gains[] =
{ -99, -73, -65, -63, -60, -58, -54, 58, 61, 63, 65, 67, 68, 70, 71, 179, 181, 182, 184, 186, 188, 191, 197 };

const int r820t_gains[] =
{ 0, 9, 14, 27, 37, 77, 87, 125, 144, 157, 166, 197, 207, 229, 254, 280, 297, 328, 338, 364, 372, 386, 402, 421, 434, 439, 445, 480, 496 };

const int r820t_if_gains[] =
{ -47, -21, 5, 35, 77, 112, 136, 149, 163, 195, 231, 265, 300, 337, 372, 408 };

// mix: 1900, 2300, 2700, 3300, 3400,
// rc: 1000, 1200, 1800, 2600, 3400,
// if channel: 2150, 2200, 2240, 2280, 2300, 2400, 2450, 2500, 2550, 2600, 2700, 2750, 2800, 2900, 2950, 3000, 3100, 3200, 3300, 3400
const int e4k_bws[] =
{ 0, 1000, 1200, 1800, 1900, 2150, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400,   5000, 10000 };

const int r820_bws[] =
//{ 0, 350, 450, 550, 700, 900, 1200, 1450, 1550, 1600, 1700, 1800, 1900, 1950, 2050, 2080, 2180, 2280, 2330, 2430, 6000, 7000, 8000 };
//{ 0, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 6000, 7000, 8000 };
//{ 0, 200, 201, 300, 301, 400, 401, 500, 501, 600, 601, 700, 701, 800, 801, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 6000, 7000, 8000 };
//{ 0, 201, 202, 290, 301, 302, 375, 401, 402, 420, 470, 501, 502, 600, 601, 602, 701, 702, 801, 802, 860, 901, 950, 1001, 1100, 1101, 1201, 1300, 1301, 1503, 1600, 1753, 1953, 6000, 7000, 8000 };
//{ 0, 202, 290, 302, 375, 420, 470, 502, 600, 602, 701, 702, 801, 802, 860, 901, 950, 1001, 1100, 1101, 1201, 1300, 1301, 1503, 1600, 1753, 1953, 6000, 7000, 8000 };
//{ 0, 290, 375, 420, 470, 600, 860, 950, 1100, 1300, 1500, 1600, 1750, 1950, 6000 /*, 7000, 8000 */ };
{ 0, 290, 375, 420, 470, 600, 860, 950, 1100, 1200, 1300, 1500, 1600, 1750, 1800, 1950, 2200, 3000, 5000 };

struct tuner_gain_t
{
	const int * gain;	// 0.1 dB steps: gain in dB = gain[] / 10
	const int num;
};


static tuner_gain_t tuner_rf_gains[] =
{
  { 0, 0 }	// tuner_type: E4000 =1, FC0012 =2, FC0013 =3, FC2580 =4, R820T =5, R828D =6
, { e4k_gains, sizeof(e4k_gains) / sizeof(e4k_gains[0]) }
, { fc12_gains, sizeof(fc12_gains) / sizeof(fc12_gains[0]) }
, { fc13_gains, sizeof(fc13_gains) / sizeof(fc13_gains[0]) }
, { 0, 0 }	// FC2580
, { r820t_gains, sizeof(r820t_gains) / sizeof(r820t_gains[0]) }
, { 0, 0 }	// R828D
};

static tuner_gain_t tuner_if_gains[] =
{
  { 0, 0 }	// tuner_type: E4000 =1, FC0012 =2, FC0013 =3, FC2580 =4, R820T =5, R828D =6
, { 0, 0 }  // E4000
, { 0, 0 }  // FC012
, { 0, 0 }  // FC013
, { 0, 0 }	// FC2580
, { r820t_if_gains, sizeof(r820t_if_gains) / sizeof(r820t_if_gains[0]) }
, { 0, 0 }	// R828D
};


struct tuner_bw_t
{
	const int * bw;	// bw in kHz: bw in Hz = bw[] * 1000
	const int num;
}
tuner_bws[] =
{
  { 0, 0 }	// tuner_type: E4000 =1, FC0012 =2, FC0013 =3, FC2580 =4, R820T =5, R828D =6
, { e4k_bws, sizeof(e4k_bws) / sizeof(e4k_bws[0]) }
, { 0, 0 }	// FC0012
, { 0, 0 }	// FC0013
, { 0, 0 }	// FC2580
, { r820_bws, sizeof(r820_bws) / sizeof(r820_bws[0]) }
, { 0, 0 }	// R828D
};



typedef struct sr {
	double value;
	TCHAR *name;
	int    valueInt;
} sr_t;


static sr_t samplerates[] = {
#if 1
	{ 225001.0, TEXT("0.225 Msps"), 225001 },				// [0]
	{ 250000.0, TEXT("0.25 Msps"), 250000 },				// [1]
	{ 264600.0, TEXT("0.265 Msps ( 88.2 kHz)"), 264600 },	// [2]
	{ 288000.0, TEXT("0.288 Msps ( 96.0 kHz)"), 288000 },	// [3]
	{ 300000.0, TEXT("0.3 Msps"), 300000 },					// [4]
#endif

	{  960000.0, TEXT("0.96 kSps (192.0 kHz)"),  960000 },	// = 5 * 192 kHz		[5]

	{ 1000000.0, TEXT("1.00 Msps"),  1000000 },				// [6]

	{ 1058400.0, TEXT("1.058 Msps (176.4 kHz)"), 1058400 },	// = 6 * 176.4 kHz		[7]
	{ 1152000.0, TEXT("1.152 Msps (192.0 kHz)"), 1152000 },	// = 6 * 192 kHz		[8]

	//{ 1200000.0, TEXT("1.20 Msps"),  1200000 },
	{ 1234800.0, TEXT("1.234 Msps (176.4 kHz)"), 1234800 },	// = 7 * 176.4 kHz		[9]
	{ 1344000.0, TEXT("1.344 Msps (192.0 kHz)"), 1344000 },	// = 7 * 192 kHz		[10]

	{ 1411200.0, TEXT("1.411 Msps (176.4 kHz)"), 1411200 },	// = 8 * 176.4 kHz		[11]

	{ 1500000.0, TEXT("1.50 Msps"), 1500000 },				// [12]

	{ 1536000.0, TEXT("1.536 Msps (192.0 kHz)"), 1536000 },	// = 8 * 192 kHz		[13]

	{ 1764000.0, TEXT("1.764 Msps (176.4 kHz)"), 1764000 },	// = 10 * 176.4 kHz		[14]

	//{ 1800000.0, TEXT("1.8 Msps"), 1800000 },
	{ 1920000.0, TEXT("1.92 Msps (192.0 kHz)"), 1920000 },	// = 10 * 192 kHz		[15]

	{ 2000000.0, TEXT("2.00 Msps"), 2000000 },				// [16]

	{ 2116800.0, TEXT("2.116 Msps (176.4 kHz)"), 2116800 },	// = 12 * 176.4 kHz		[17]
	{ 2304000.0, TEXT("2.304 Msps (192.0 kHz)"), 2304000 },	// = 12 * 192 kHz		[18]

	{ 2400000.0, TEXT("2.4 Msps"),  2400000 },

	// loss of samples > 2.4 Msps

	{ 2469600.0, TEXT("2.469 Msps (44.1 kHz, rtl_test!)"), 2469600 },	// = 14 * 176.4 kHz		[19]

	{ 2500000.0, TEXT("2.50 Msps (rtl_test!)"), 2500000 },				// [20]

	{ 2560000.0, TEXT("2.56 Msps"), 2560000 },				// [21]


	{ 2646000.0, TEXT("2.646 Msps (44.1 kHz, rtl_test!)"), 2646000 },	// = 15 * 176.4 kHz		[22]

	{ 2688000.0, TEXT("2.688 Msps (48.0 kHz, rtl_test!)"), 2688000 },	// = 14 * 192 kHz		[23]

	{ 2822400.0, TEXT("2.822 Msps (44.1 kHz, rtl_test!)"), 2822400 },	// = 16 * 176.4 kHz		[24]

	{ 2880000.0, TEXT("2.88 Msps (48.0 kHz, rtl_test!)"), 2880000 },	// = 15 * 192 kHz		[25]

#if 0
	{ 3000000.0, TEXT("3.00 Msps (rtl_test!!!)"), 3000000 },

	{ 3072000.0, TEXT("3.072 Msps (192.0 kHz, rtl_test!!!)"), 3072000 },	// 16 * 192 kHz			[17]
	{ 3090000.0, TEXT("3.09 Msps (rtl_test!!!)"), 3090000 },
	{ 3100000.0, TEXT("3.1 Msps (rtl_test!!!)"), 3100000 },
#endif

	{ 3200000.0, TEXT("3.2 Msps (rtl_test!!!)"), 3200000 }
};

static const int n_srates = sizeof(samplerates) / sizeof(samplerates[0]);


static TCHAR* directS[] = {
	TEXT("I/Q - sampling of tuner output"),
	TEXT("pin I: aliases 0 - 14.4 - 28.8 MHz!"),
	TEXT("pin Q: aliases 0 - 14.4 - 28.8 MHz! (V3)")
};


static const int * bandwidths = 0;
static int n_bandwidths = 0;			// tuner_bws[]

static const int * rf_gains = 0;
static int n_rf_gains = 0;			// tuner_rf_gains[]

static const int * if_gains = 0;
static int n_if_gains = 0;			// tuner_if_gains[]


static union
{
	uint8_t ac[8];			// write 0, 1, 2, [3]
	uint32_t ui[2];			// write 0, [1]
} rtl_tcp_cmd;

static volatile union
{
	int8_t		ac[12];		// 0 .. 3 == "RTL0"
	uint32_t	ui[3];		// [1] = tuner_type; [2] = tuner_gain_count
	// tuner_type: E4000 =1, FC0012 =2, FC0013 =3, FC2580 =4, R820T =5, R828D =6
} rtl_tcp_dongle_info;

static volatile uint32_t	tunerNo = 0;
static volatile uint32_t	numTunerGains = 0;

static volatile bool GotTunerInfo = false;


static int maxDecimation = 0;

static bool SDRsupportsLogging = false;
static bool SDRsupportsSamplePCMU8 = false;
static bool SDRsupportsSampleFormats = false;


#define MAX_BUFFER_LEN	(256*1024)
#define NUM_BUFFERS_BEFORE_CALLBACK		( MAX_DECIMATIONS + 1 )

static bool rcvBufsAllocated = false;
static short * short_buf = 0;
static uint8_t * rcvBuf[NUM_BUFFERS_BEFORE_CALLBACK + 1] = { 0 };

static volatile int somewhat_changed = 0;
    // 1 == freq
    // 2 == srate
    // 4 == gain
    // 8 == tuner rf agc
    // 16 == rtl agc
    // 32 == direct sampling mode
    // 64 == offset Tuning (E4000)
    // 128 == freq corr ppm
    // 256 == tuner bandwidth
    // 512 == decimation
    // 1024 == tuner sideband
    // 2048 == tuner band center
    // 4096 == tuner if agc / if gain
    // 8192 == gpio changed
    // 16384 == band_changed
    // 32768 == command everything

static volatile int64_t last_LO_freq = 100000000;
static volatile int64_t new_LO_freq = 100000000;

static volatile int64_t old_tuneFreq = 0;
static volatile int64_t new_tuneFreq = 0;  // absolute (RF) frequency of tuned frequency

static volatile int64_t retune_value = 0;
static int retune_counter = 0;
static volatile bool retune_freq = false;

static volatile int last_srate_idx = 18;
static volatile int new_srate_idx = 18;		// default = 2.3 MSps

static volatile int last_TunerBW = 0;		// 0 == automatic, sonst in Hz
static volatile int new_TunerBW = 0;		// n_bandwidths = bandwidths[]; nearestBwIdx()

static volatile int last_Decimation = 0;
static volatile int new_Decimation = 0;

static volatile int last_rf_gain = 1;
static volatile int new_rf_gain = 1;

static volatile int last_if_gain = 1;
static volatile int new_if_gain = 1;

static volatile int last_if_gain_idx = 1;
static volatile int new_if_gain_idx = 1;

static volatile int last_TunerRF_AGC = 1;	// 0 == off/manual, 1 == on/automatic
static volatile int new_TunerRF_AGC = 1;

static volatile int last_TunerIF_AGC = 1;	// 0 == off/manual, 1 == on/automatic
static volatile int new_TunerIF_AGC = 1;

static volatile int last_RTLAGC = 1;
static volatile int new_RTLAGC = 1;

static volatile int last_DirectSampling = 0;
static volatile int new_DirectSampling = 0;

static volatile int last_BandCenter_Sel = 0;
static volatile int new_BandCenter_Sel = 0;

static volatile int last_BandCenter_LO_Delta = 0;
static volatile int new_BandCenter_LO_Delta = 0;

static volatile int last_OffsetTuning = 0;
static volatile int new_OffsetTuning = 0;

static volatile int last_USB_Sideband = 0;
static volatile int new_USB_Sideband = 0;

#define NUM_GPIO_BUTTONS  5
//                                                 A  B  C  D  E
static volatile int GPIO_pin[NUM_GPIO_BUTTONS] = { 0, 1, 2, 4, 5 };
static volatile int GPIO_inv[NUM_GPIO_BUTTONS] = { 0, 0, 0, 0, 0 };
static volatile int GPIO_en[NUM_GPIO_BUTTONS] = { 1, 1, 1, 1, 1 };
static char GPIO_txt[NUM_GPIO_BUTTONS][16] = { "4.5V BIAS TEE", "GPIO1 / PIN32", "GPIO2 / PIN31", "GPIO4 / PIN30", "GPIO5 / PIN29" };
static volatile int last_GPIO[NUM_GPIO_BUTTONS] = { -9, -9, -9, -9, -9 };
static volatile int new_GPIO[NUM_GPIO_BUTTONS] = { 0 };

static volatile int last_FreqCorrPPM = 0;
static volatile int new_FreqCorrPPM = 0;

static volatile int bufferSizeIdx = 6;// 64 kBytes
static volatile int buffer_len = buffer_sizes[bufferSizeIdx];


static char RTL_TCP_IPAddr[32] = "127.0.0.1";
static int RTL_TCP_PortNo = 1234;

static volatile int AutoReConnect = 1;
static volatile int PersistentConnection = 1;

static int ASyncSocketIO = 1;
static int SleepMillisWaitingForData = 1;

static int HDSDR_AGC=2;


typedef struct {
	char vendor[256], product[256], serial[256], name[256];
} device;

device connected_device;

// Thread handle
volatile bool terminateThread = false;
volatile bool ThreadStreamToSDR = false;
volatile bool commandEverything = true;
static bool GUIDebugConnection = false;
static volatile HANDLE worker_handle=INVALID_HANDLE_VALUE;
void ThreadProc(void * param);


int Start_Thread();
int Stop_Thread();

/* ExtIO Callback */
extern pfnExtIOCallback gpfnExtIOCallbackPtr = NULL;

// error message, with "const char*" in IQdata,
//   intended for a log file  AND  a message box
#define SDRLOG( A, TEXT )	do { if ( gpfnExtIOCallbackPtr ) gpfnExtIOCallbackPtr(-1, A, 0, TEXT ); } while (0)


static INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
static HWND h_dialog=NULL;


static bool transmitTcpCmd(CActiveSocket &conn, uint8_t cmdId, uint32_t value)
{
	rtl_tcp_cmd.ac[3] = cmdId;
	rtl_tcp_cmd.ui[1] = htonl(value);
	int iSent = conn.Send(&rtl_tcp_cmd.ac[3], 5);
	return (5 == iSent);
}

static int nearestSrateIdx(int srate)
{
	if (srate <= 0)
		return 0;
	else if (srate <= samplerates[1].valueInt)
		return 1;
	else if (srate >= samplerates[n_srates - 1].valueInt)
		return n_srates - 1;

	int nearest_idx = 1;
	int nearest_dist = 10000000;
	for (int idx = 0; idx < n_srates; ++idx)
	{
		int dist = abs(srate - samplerates[idx].valueInt);
		if (dist < nearest_dist)
		{
			nearest_idx = idx;
			nearest_dist = dist;
		}
	}
	return nearest_idx;
}

static int nearestBwIdx(int bw)
{
	if (bw <= 0 || n_bandwidths <= 0)
		return 0;
	else if (bw <= bandwidths[1])
		return 1;
	else if (bw >= bandwidths[n_bandwidths - 1])
		return n_bandwidths - 1;

	int nearest_idx = 1;
	int nearest_dist = 10000000;
	for (int idx = 1; idx < n_bandwidths; ++idx)
	{
		int dist = abs(bw - bandwidths[idx]);
		if (dist < nearest_dist)
		{
			nearest_idx = idx;
			nearest_dist = dist;
		}
	}
	return nearest_idx;
}

static int nearestGainIdx(int gain, const int * gains, const int n_gains)
{
    if (n_gains <= 0)
        return 0;
    else if (gain <= gains[0])
        return 0;
    else if (gain >= gains[n_gains - 1])
        return n_gains - 1;
    
    int nearest_idx = 0;
    int nearest_dist = 10000000;
    for (int idx = 0; idx < n_gains; ++idx)
    {
        int dist = abs(gain - gains[idx]);
        if (dist < nearest_dist)
        {
            nearest_idx = idx;
            nearest_dist = dist;
        }
    }
    return nearest_idx;
}


extern "C"
bool  LIBRTL_API __stdcall InitHW(char *name, char *model, int& type)
{
    init_toml_config();     // process as early as possible, but that depends on SDR software

	strcpy_s( connected_device.vendor, 256, "Realtek" );
	strcpy_s( connected_device.product,15, "RTL2832" );
	strcpy_s( connected_device.serial, 256, "NA" );
	strcpy_s( connected_device.name,   63, "Realtek RTL2832" );

	strcpy_s(name,63, connected_device.name);
	strcpy_s(model,15,connected_device.product);
	name[63]=0;
	model[15]=0;

	extHWtype = exthwUSBdata16;  /* 16-bit samples */

	if (!SDRsupportsSamplePCMU8)
		extHWtype = exthwUSBdata16;  /* 16-bit samples */
	else
	{
		if (MAX_DECIMATIONS == 1)
			extHWtype = exthwUSBdataU8;		// 8bit samples are sufficient when not using decimation
		else if (!SDRsupportsSampleFormats)
			extHWtype = exthwUSBdata16;		// with decimation 16-bit samples are necessary
		else
		{
			// dynamic extSDR_supports_SampleFormats and extSDR_supports_PCMU8 supported
			// just depends on current decimation
			if (new_Decimation <= 1)
				extHWtype = exthwUSBdataU8;		// 8bit samples are sufficient when not using decimation
			else
				extHWtype = exthwUSBdata16;		// with decimation 16-bit samples are necessary
		}
#if ALWAYS_PCMU8
		extHWtype = exthwUSBdataU8;		// 8bit samples are sufficient when not using decimation
#elif ALWAYS_PCM16
		extHWtype = exthwUSBdata16;		// with decimation 16-bit samples are necessary
#endif
	}

	if (exthwUSBdata16 == extHWtype)
		SDRLOG(extHw_MSG_DEBUG, "InitHW() with sample type PCM16");
	else if (exthwUSBdataU8 == extHWtype)
		SDRLOG(extHw_MSG_DEBUG, "InitHW() with sample type PCMU8");

	type = extHWtype;

	return TRUE;
}

extern "C"
int LIBRTL_API __stdcall GetStatus()
{
	/* dummy function */
    return 0;
}

extern "C"
bool  LIBRTL_API __stdcall OpenHW()
{
	SDRLOG(extHw_MSG_DEBUG, "OpenHW()");

	h_dialog=CreateDialog(hInst, MAKEINTRESOURCE(IDD_RTL_SETTINGS), NULL, (DLGPROC)MainDlgProc);
	if (h_dialog)
		ShowWindow(h_dialog,SW_HIDE);

	if (PersistentConnection)
	{
		SDRLOG(extHw_MSG_DEBUG, "OpenHW() starts thread (persistent connection)");

		ThreadStreamToSDR = false;
		if (Start_Thread() < 0)
			return FALSE;
	}

	return TRUE;
}


static TCHAR band_disp_text[255] = { 0 };


static int _setHwLO_check_bands(int64_t freq)
{
    if (new_LO_freq == freq || !h_dialog)
        return 0;

    const BandAction* new_band = update_band_action(double(freq));
    if (!new_band)
        return 0;

    // we are now moving into a new band with some defined action(s)
    const BandAction& ba = *new_band;   // have a shorter alias
    bool post_msg = false;      // update all fields in GUI dialog?
    int changed_flags = 0;

    if (ba.name.has_value())
        _stprintf_s(band_disp_text, 255, TEXT("Band: %s"), ba.name.value().c_str());
    else
        _stprintf_s(band_disp_text, 255, TEXT("Band: %s"), ba.id.c_str());
    post_msg = true;  // post anyway - to change displayed band name

    if (ba.sampling_mode)
    {
        if (ba.sampling_mode.value() == 'I')
            new_DirectSampling = 1;
        else if (ba.sampling_mode.value() == 'Q')
            new_DirectSampling = 2;
        else if (ba.sampling_mode.value() == 'C')
            new_DirectSampling = 0;
        changed_flags |= 32; // direct sampling mode;
    }

    if (ba.rtl_digital_agc)
    {
        new_RTLAGC = ba.rtl_digital_agc.value() ? 1 : 0;
        changed_flags |= 16; // rtl agc
    }

    if (ba.tuner_rf_agc)
    {
        new_TunerRF_AGC = ba.tuner_rf_agc.value() ? 1 : 0;
        changed_flags |= 8;
    }

    if (ba.tuner_if_agc)
    {
        new_TunerIF_AGC = ba.tuner_if_agc.value() ? 1 : 0;
        changed_flags |= 4096;
    }

    if (ba.tuning_sideband)
    {
        if (ba.tuning_sideband.value() == 'L')
            new_USB_Sideband = 0;
        else if (ba.tuning_sideband.value() == 'U')
            new_USB_Sideband = 1;
        changed_flags |= 1024;
    }

    if (ba.gpio_button0)
        new_GPIO[0] = ba.gpio_button0.value() ? 1 : 0;
    if (ba.gpio_button1)
        new_GPIO[1] = ba.gpio_button0.value() ? 1 : 0;
    if (ba.gpio_button2)
        new_GPIO[2] = ba.gpio_button0.value() ? 1 : 0;
    if (ba.gpio_button3)
        new_GPIO[3] = ba.gpio_button0.value() ? 1 : 0;
    if (ba.gpio_button4)
        new_GPIO[4] = ba.gpio_button0.value() ? 1 : 0;
    if (ba.gpio_button0 || ba.gpio_button1 || ba.gpio_button2 || ba.gpio_button3 || ba.gpio_button4)
        changed_flags |= 1024;

    if (ba.tuner_rf_gain_db)
    {
        new_TunerRF_AGC = 0;    // also deactivate AGC
        const int rf_gain_tenth_db = int(ba.tuner_rf_gain_db.value() * 10.0);
        if (GotTunerInfo && tunerNo < (sizeof(tuner_rf_gains) / sizeof(tuner_rf_gains[0])))
        {
            int gainIdx = nearestGainIdx(rf_gain_tenth_db, tuner_rf_gains[tunerNo].gain, tuner_rf_gains[tunerNo].num);
            if (gainIdx >= 0 && gainIdx < tuner_rf_gains[tunerNo].num)
                new_rf_gain = tuner_rf_gains[tunerNo].gain[gainIdx];
        }
        changed_flags |= (8 | 4);
    }
    else if (ba.tuner_rf_gain_db && GotTunerInfo && tunerNo < (sizeof(tuner_rf_gains) / sizeof(tuner_rf_gains[0])))
    {
        new_TunerRF_AGC = 0;    // also deactivate AGC
        const int rf_gain_tenth_db = int(ba.tuner_rf_gain_db.value() * 10.0);
        int gainIdx = nearestGainIdx(rf_gain_tenth_db, tuner_rf_gains[tunerNo].gain, tuner_rf_gains[tunerNo].num);
        SetAttenuator(gainIdx);
    }

    if (ba.tuner_if_gain_db)
    {
        new_TunerIF_AGC = 0;    // also deactivate AGC
        int if_gain_tenth_db = int(ba.tuner_if_gain_db.value() * 10.0);
        if (GotTunerInfo && tunerNo < (sizeof(tuner_if_gains) / sizeof(tuner_if_gains[0])))
        {
            new_if_gain_idx = nearestGainIdx(if_gain_tenth_db, tuner_if_gains[tunerNo].gain, tuner_if_gains[tunerNo].num);
            if (new_if_gain_idx >= 0 && new_if_gain_idx < tuner_if_gains[tunerNo].num)
                new_if_gain = tuner_if_gains[tunerNo].gain[new_if_gain_idx];

        }
        changed_flags |= 4096;
    }
    else if (ba.tuner_if_gain_db && GotTunerInfo && tunerNo < (sizeof(tuner_if_gains) / sizeof(tuner_if_gains[0])))
    {
        new_TunerIF_AGC = 0;    // also deactivate AGC
        const int if_gain_tenth_db = int(ba.tuner_if_gain_db.value() * 10.0);

        int if_gain_idx = nearestGainIdx(if_gain_tenth_db, tuner_if_gains[tunerNo].gain, tuner_if_gains[tunerNo].num);
        ExtIoSetMGC(if_gain_idx);
    }


    if (post_msg || changed_flags)  // update GUI fields on changes
        PostMessage(h_dialog, WM_USER + 42, (WPARAM)0, (LPARAM)0);

    //if (changed_flags)
    //    changed_flags |= 32768; // cmd_everything ! - if there's anything to command

    return changed_flags;
}


extern "C"
long LIBRTL_API __stdcall SetHWLO(long freq)
{
    const int change_flags = _setHwLO_check_bands(freq);
    new_LO_freq = freq; // +new_BandCenter_LO_Delta;
    somewhat_changed |= (change_flags | 1);
    return 0;
}


extern "C"
int64_t LIBRTL_API __stdcall SetHWLO64(int64_t freq)
{
  const int change_flags = _setHwLO_check_bands(freq);
  new_LO_freq = freq; // +new_BandCenter_LO_Delta;
  somewhat_changed |= (change_flags | 1);
  return 0;
}




static FILE * tuneFile = NULL;

extern "C"
void LIBRTL_API __stdcall TuneChanged64(int64_t tunefreq)
{
  new_tuneFreq = tunefreq;
}

extern "C"
void LIBRTL_API __stdcall TuneChanged(long tunefreq)
{
  new_tuneFreq = tunefreq;
}

extern "C"
int64_t LIBRTL_API __stdcall GetTune64(void)
{
  return new_tuneFreq;
}

extern "C"
long LIBRTL_API __stdcall GetTune(void)
{
  return (long)(new_tuneFreq);
}


extern "C"
int LIBRTL_API __stdcall StartHW(long freq)
{
	char acMsg[256];
	SDRLOG(extHw_MSG_DEBUG, "StartHW()");

	if (SDRsupportsSamplePCMU8)
		SDRLOG(extHw_MSG_DEBUG, "StartHW(): PCMU8 is supported");
	else
		SDRLOG(extHw_MSG_DEBUG, "StartHW(): PCMU8 is NOT supported");

	if (exthwUSBdata16 == extHWtype)
		SDRLOG(extHw_MSG_DEBUG, "StartHW(): using sample type PCM16");
	else if (exthwUSBdataU8 == extHWtype)
		SDRLOG(extHw_MSG_DEBUG, "StartHW(): using sample type PCMU8");
	else
		SDRLOG(extHw_MSG_DEBUG, "StartHW(): using 'other' sample type - NOT PCMU8 or PCM16!");

	commandEverything = true;
	ThreadStreamToSDR = true;
	if ( Start_Thread() < 0 )
		return -1;

    SetHWLO(freq);

	if (h_dialog)
	{
		EnableWindow(GetDlgItem(h_dialog, IDC_IP_PORT), FALSE);
		EnableWindow(GetDlgItem(h_dialog, IDC_DIRECT), FALSE);
	}

	// blockSize is independent of decimation!
	// else, we get just 64 = 512 / 8 I/Q Samples with 1 kB bufferSize!
	int numIQpairs = buffer_len / 2;

	snprintf(acMsg, 255, "StartHW() = %d. Callback will deliver %d I/Q pairs per call", numIQpairs, numIQpairs);
	SDRLOG(extHw_MSG_DEBUG, acMsg);

	return numIQpairs;
}


extern "C"
int64_t LIBRTL_API __stdcall GetHWLO64()
{
  return new_LO_freq;
}

extern "C"
long LIBRTL_API __stdcall GetHWLO()
{
  return (long)new_LO_freq;
}


extern "C"
long LIBRTL_API __stdcall GetHWSR()
{
	long sr = long(samplerates[new_srate_idx].valueInt);
#if ( FULL_DECIMATION )
	sr /= new_Decimation;
#endif
	return sr;
}

extern "C"
int LIBRTL_API __stdcall ExtIoGetSrates( int srate_idx, double * samplerate )
{
	if (srate_idx < n_srates)
	{
#if ( FULL_DECIMATION )
		*samplerate = samplerates[srate_idx].value / new_Decimation;
#else
		*samplerate = samplerates[srate_idx].value;
#endif
		return 0;
	}
	return 1;	// ERROR
}

extern "C"
long LIBRTL_API __stdcall ExtIoGetBandwidth(int srate_idx)
{
	if (srate_idx < n_srates)
	{
		// ~ 3/4 of spectrum usable
		long bw = samplerates[srate_idx].valueInt * 3L / ( new_Decimation * 4L );
		if (new_TunerBW && new_TunerBW * 1000L < bw)
			bw = new_TunerBW * 1000L;
		return bw;
	}
	return 0;	// ERROR
}

extern "C"
int  LIBRTL_API __stdcall ExtIoGetActualSrateIdx(void)
{
	return new_srate_idx;
}

extern "C"
int  LIBRTL_API __stdcall ExtIoSetSrate( int srate_idx )
{
    if (srate_idx >= 0 && srate_idx < n_srates)
    {
        new_srate_idx = srate_idx;
        somewhat_changed |= 2;
        if (h_dialog)
            ComboBox_SetCurSel(GetDlgItem(h_dialog,IDC_SAMPLERATE),srate_idx);
        EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SampleRate);  // Signal application
        return 0;
    }
    return 1;	// ERROR
}

extern "C"
int  LIBRTL_API __stdcall GetAttenuators( int atten_idx, float * attenuation )
{
    if ( atten_idx < n_rf_gains )
    {
        *attenuation= rf_gains[atten_idx]/10.0F;
        return 0;
    }
    return 1;	// End or Error
}

extern "C"
int  LIBRTL_API __stdcall GetActualAttIdx(void)
{
    for (int i=0;i<n_rf_gains;i++)
        if (new_rf_gain == rf_gains[i])
            return i;
    return -1;
}

extern "C"
int  LIBRTL_API __stdcall SetAttenuator( int atten_idx )
{
    if ( atten_idx<0 || atten_idx > n_rf_gains )
        return -1;

    int pos=rf_gains[atten_idx];

    if (h_dialog)
    {
        SendMessage(GetDlgItem(h_dialog, IDC_RF_GAIN_SLIDER), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-pos);
        
        if (Button_GetCheck(GetDlgItem(h_dialog, IDC_TUNER_RF_AGC)) == BST_UNCHECKED)
        {
            TCHAR str[255];
            _stprintf_s(str, 255, TEXT("%2.1f  dB"), (float)pos / 10);
            Static_SetText(GetDlgItem(h_dialog, IDC_TUNER_RF_GAIN_VALUE), str);
            new_rf_gain = pos;
            somewhat_changed |= 4;
        }
    }

    new_rf_gain = pos;
    return 0;
}


extern "C"
int  LIBRTL_API __stdcall ExtIoGetMGCs(int mgc_idx, float * gain)
{
    if ( mgc_idx < n_if_gains )
    {
        *gain= if_gains[mgc_idx]/10.0F;
        return 0;
    }
    return 1;	// End or Error
}

extern "C"
int  LIBRTL_API __stdcall ExtIoGetActualMgcIdx(void)
{
    for (int i=0;i<n_if_gains;i++)
        if (new_if_gain == if_gains[i])
            return i;
    return -1;
}

extern "C"
int  LIBRTL_API __stdcall ExtIoSetMGC(int mgc_idx)
{
    if ( mgc_idx<0 || mgc_idx > n_if_gains )
        return -1;

    int pos=if_gains[mgc_idx];

    if (h_dialog)
    {
        SendMessage(GetDlgItem(h_dialog, IDC_IF_GAIN_SLIDER), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-pos);
        
        if (Button_GetCheck(GetDlgItem(h_dialog, IDC_TUNER_IF_AGC)) == BST_UNCHECKED)
        {
            TCHAR str[255];
            _stprintf_s(str, 255, TEXT("%2.1f  dB"), (float)pos / 10);
            Static_SetText(GetDlgItem(h_dialog, IDC_TUNER_IF_GAIN_VALUE), str);
        }
    }

    new_if_gain = pos;
    new_if_gain_idx = mgc_idx;
    somewhat_changed |= 4096;
    return 0;

}


#if WITH_AGCS

extern "C"
int   LIBRTL_API __stdcall ExtIoGetAGCs( int agc_idx, char * text )
{
    switch ( agc_idx )
    {
        case 0:   snprintf( text, 16, "%s", "IF" );      return 0;
        case 1:   snprintf( text, 16, "%s", "RF:A IF" ); return 0;
        case 2:   snprintf( text, 16, "%s", "AGC" );     return 0;
        default:  return -1;	// ERROR
    }
    return -1;	// ERROR
}

extern "C"
int   LIBRTL_API __stdcall ExtIoGetActualAGCidx (void)
{
	return HDSDR_AGC;
}

extern "C"
int   LIBRTL_API __stdcall ExtIoSetAGC (int agc_idx)
{
    WPARAM RF_val = BST_UNCHECKED; // BST_CHECKED;
    WPARAM IF_val = BST_UNCHECKED; // BST_CHECKED;
    //WPARAM DIG_val = BST_UNCHECKED; // BST_CHECKED;
    HDSDR_AGC = agc_idx;
    switch ( agc_idx )
    {
        case 0:   break; // "IF"  
        case 1:   RF_val = BST_CHECKED;  break; // "RF:A IF"
        default:
        case 2:   RF_val = IF_val = BST_CHECKED;  break;  // "AGC"
    }
    SendMessage(GetDlgItem(h_dialog, IDC_TUNER_RF_AGC), BM_SETCHECK, RF_val, NULL);
    SendMessage(GetDlgItem(h_dialog, IDC_TUNER_IF_AGC), BM_SETCHECK, IF_val, NULL);
    //SendMessage(GetDlgItem(h_dialog, IDC_RTL_DIG_AGC), BM_SETCHECK, DIG_val, NULL);
    return 0;
}

extern "C"
int   LIBRTL_API __stdcall ExtIoShowMGC(int agc_idx)
{
    // return 1 - shows MGC == IF slider
    switch ( agc_idx )
    {
        default:
        case 0:   return 1;  // "IF"  
        case 1:   return 1;  // "RF AGC IF"
        case 2:   return 0;  // "RF+IF AGC"
    }
}

#endif /* WITH_AGCS */

//---------------------------------------------------------------------------

enum class Setting {
  ID = 0
, RTL_TCP_IP
, RTL_TCP_PORTNO
, AUTO_RECONNECT
, PERSISTENT_CONN
, SRATE_IDX
, TUNER_BW
, TUNER_RF_AGC
, RTL_DIG_AGC
, CALIB_PPM
, MANUAL_RF_GAIN
, BUFFER_SIZE_IDX
, E4K_OFFSET_TUNE
, DIRECT_SAMPLING_MODE
, ASYNC_SOCKET_IO
, SLEEP_MILLIS_FOR_DATA
, DECIMATION
, TUNER_IF_AGC              // int new_TunerIF_AGC = 1
, MANUAL_IF_GAIN_IDX        // int new_if_gain_idx = 1
, MANUAL_IF_GAIN_VAL        // int new_if_gain = 1
, R820T_BAND_CENTER_SEL     // int new_BandCenter_Sel = 0
, R820T_BAND_CENTER_DELTA   // int new_BandCenter_LO_Delta = 0
, R820T_USB_SIDEBAND        // int new_USB_Sideband = 0

, RTL_GPIO_A_PIN_EN
, RTL_GPIO_A_INVERT
, RTL_GPIO_A_VALUE
, RTL_GPIO_A_LABEL

, RTL_GPIO_B_PIN_EN
, RTL_GPIO_B_INVERT
, RTL_GPIO_B_VALUE
, RTL_GPIO_B_LABEL

, RTL_GPIO_C_PIN_EN
, RTL_GPIO_C_INVERT
, RTL_GPIO_C_VALUE
, RTL_GPIO_C_LABEL

, RTL_GPIO_D_PIN_EN
, RTL_GPIO_D_INVERT
, RTL_GPIO_D_VALUE
, RTL_GPIO_D_LABEL

, RTL_GPIO_E_PIN_EN
, RTL_GPIO_E_INVERT
, RTL_GPIO_E_VALUE
, RTL_GPIO_E_LABEL

, NUM   // Last One == Amount
};


extern "C"
int   LIBRTL_API __stdcall ExtIoGetSetting( int idx, char * description, char * value )
{
  switch ( Setting(idx) )
  {
  case Setting::ID:
    snprintf( description, 1024, "%s", "Identifier" );
    snprintf( value, 1024, "%s", SETTINGS_IDENTIFIER );
    return 0;
  case Setting::RTL_TCP_IP:
    snprintf(description, 1024, "%s", "RTL_TCP IP-Address");
    snprintf(value, 1024, "%s", RTL_TCP_IPAddr);
    return 0;
  case Setting::RTL_TCP_PORTNO:
    snprintf(description, 1024, "%s", "RTL_TCP Portnumber");
    snprintf(value, 1024, "%d", RTL_TCP_PortNo);
    return 0;
  case Setting::AUTO_RECONNECT:
    snprintf(description, 1024, "%s", "Automatic_ReConnect");
    snprintf(value, 1024, "%d", AutoReConnect);
    return 0;
  case Setting::PERSISTENT_CONN:
    snprintf(description, 1024, "%s", "Persistent_Connection");
    snprintf(value, 1024, "%d", PersistentConnection);
    return 0;
  case Setting::SRATE_IDX:
    snprintf( description, 1024, "%s", "SampleRateIdx" );
    snprintf(value, 1024, "%d", new_srate_idx);
    return 0;
  case Setting::TUNER_BW:
    snprintf(description, 1024, "%s", "TunerBandwidth in kHz (only few tuner models) - 0 for automatic");
    snprintf(value, 1024, "%d", new_TunerBW);
    return 0;
  case Setting::TUNER_RF_AGC:
    snprintf( description, 1024, "%s", "Tuner_RF_AGC" );
    snprintf(value, 1024, "%d", new_TunerRF_AGC);
    return 0;
  case Setting::RTL_DIG_AGC:
    snprintf( description, 1024, "%s", "RTL_AGC" );
    snprintf(value, 1024, "%d", new_RTLAGC);
    return 0;
  case Setting::CALIB_PPM:
    snprintf( description, 1024, "%s", "Frequency_Correction" );
    snprintf(value, 1024, "%d", new_FreqCorrPPM);
    return 0;
  case Setting::MANUAL_RF_GAIN:
    snprintf( description, 1024, "%s", "Tuner_RF_Gain" );
    snprintf(value, 1024, "%d", new_rf_gain);
    return 0;
  case Setting::BUFFER_SIZE_IDX:
    snprintf( description, 1024, "%s", "Buffer_Size" );
    snprintf(value, 1024, "%d", bufferSizeIdx);
    return 0;
  case Setting::E4K_OFFSET_TUNE:
    snprintf( description, 1024, "%s", "Tuner-E4000_Offset_Tuning" );
    snprintf(value, 1024, "%d", new_OffsetTuning);
    return 0;
  case Setting::DIRECT_SAMPLING_MODE:
    snprintf( description, 1024, "%s", "Direct_Sampling" );
    snprintf(value, 1024, "%d", new_DirectSampling);
    return 0;
  case Setting::ASYNC_SOCKET_IO:
    snprintf(description, 1024, "%s", "Use Asynchronous I/O on Socket connection");
    snprintf(value, 1024, "%d", ASyncSocketIO);
    return 0;
  case Setting::SLEEP_MILLIS_FOR_DATA:
    snprintf(description, 1024, "%s", "number of Milliseconds to Sleep before trying to receive new data");
    snprintf(value, 1024, "%d", SleepMillisWaitingForData);
    return 0;
  case Setting::DECIMATION:
    snprintf(description, 1024, "%s", "Decimation Factor for Sample Rate");
    snprintf(value, 1024, "%d", new_Decimation);
    return 0;

  case Setting::TUNER_IF_AGC:             // int new_TunerIF_AGC = 1
    snprintf(description, 1024, "%s", "R820T/2_IF_AGC");
    snprintf(value, 1024, "%d", new_TunerIF_AGC);
    return 0;
  case Setting::MANUAL_IF_GAIN_IDX:       // int new_if_gain_idx = 1
    snprintf(description, 1024, "%s", "R820T/2_manual_IF_GAIN_idx");
    snprintf(value, 1024, "%d", new_if_gain_idx);
    return 0;
  case Setting::MANUAL_IF_GAIN_VAL:       // int new_if_gain = 1
    snprintf(description, 1024, "%s", "R820T/2_manual_IF_GAIN_value");
    snprintf(value, 1024, "%d", new_if_gain);
    return 0;
  case Setting::R820T_BAND_CENTER_SEL:    // int new_BandCenter_Sel = 0
    snprintf(description, 1024, "%s", "R820T/2_Band_Center_Selection");
    snprintf(value, 1024, "%d", new_BandCenter_Sel);
    return 0;
  case Setting::R820T_BAND_CENTER_DELTA:  // int new_BandCenter_LO_Delta = 0
    snprintf(description, 1024, "%s", "R820T/2_Band_Center_in_Hz");
    snprintf(value, 1024, "%d", new_BandCenter_LO_Delta);
    return 0;
  case Setting::R820T_USB_SIDEBAND:       // int new_USB_Sideband = 0
    snprintf(description, 1024, "%s", "R820T/2_Tune_Upper_Sideband");
    snprintf(value, 1024, "%d", new_USB_Sideband);
    return 0;

  case Setting::RTL_GPIO_A_PIN_EN:
    snprintf(description, 1024, "%s", "GPIO Button A: Pin Number (0 .. 7), -1 for BiasT, other values to disable");
    snprintf(value, 1024, "%d", GPIO_en[0] ? GPIO_pin[0] : (-10 - GPIO_pin[0]) );
    return 0;
  case Setting::RTL_GPIO_A_INVERT:
    snprintf(description, 1024, "%s", "GPIO Button A: Invert Pin Value?: 0 = value as CheckBox. 1 = inverted");
    snprintf(value, 1024, "%d", GPIO_inv[0]);
    return 0;
  case Setting::RTL_GPIO_A_VALUE:
    snprintf(description, 1024, "%s", "GPIO Button A: Checkbox Value?: 0 = unchecked. 1 = checked");
    snprintf(value, 1024, "%d", new_GPIO[0]);
    return 0;
  case Setting::RTL_GPIO_A_LABEL:
    snprintf(description, 1024, "%s", "GPIO Button A: Label to be shown at CheckBox - max size 16");
    snprintf(value, 1024, "%s", &GPIO_txt[0][0]);
    return 0;
  case Setting::RTL_GPIO_B_PIN_EN:
    snprintf(description, 1024, "%s", "GPIO Button B: Pin Number (0 .. 7), -1 for BiasT, other values to disable");
    snprintf(value, 1024, "%d", GPIO_en[1] ? GPIO_pin[1] : (-10 - GPIO_pin[1]) );
    return 0;
  case Setting::RTL_GPIO_B_INVERT:
    snprintf(description, 1024, "%s", "GPIO Button B: Invert Pin Value?: 0 = value as CheckBox. 1 = inverted");
    snprintf(value, 1024, "%d", GPIO_inv[1]);
    return 0;
  case Setting::RTL_GPIO_B_VALUE:
    snprintf(description, 1024, "%s", "GPIO Button B: Checkbox Value?: 0 = unchecked. 1 = checked");
    snprintf(value, 1024, "%d", new_GPIO[1]);
    return 0;
  case Setting::RTL_GPIO_B_LABEL:
    snprintf(description, 1024, "%s", "GPIO Button B: Label to be shown at CheckBox - max size 16");
    snprintf(value, 1024, "%s", &GPIO_txt[1][0]);
    return 0;
  case Setting::RTL_GPIO_C_PIN_EN:
    snprintf(description, 1024, "%s", "GPIO Button C: Pin Number (0 .. 7), -1 for BiasT, other values to disable");
    snprintf(value, 1024, "%d", GPIO_en[2] ? GPIO_pin[2] : (-10 - GPIO_pin[2]) );
    return 0;
  case Setting::RTL_GPIO_C_INVERT:
    snprintf(description, 1024, "%s", "GPIO Button C: Invert Pin Value?: 0 = value as CheckBox. 1 = inverted");
    snprintf(value, 1024, "%d", GPIO_inv[2]);
    return 0;
  case Setting::RTL_GPIO_C_VALUE:
    snprintf(description, 1024, "%s", "GPIO Button C: Checkbox Value?: 0 = unchecked. 1 = checked");
    snprintf(value, 1024, "%d", new_GPIO[2]);
    return 0;
  case Setting::RTL_GPIO_C_LABEL:
    snprintf(description, 1024, "%s", "GPIO Button C: Label to be shown at CheckBox - max size 16");
    snprintf(value, 1024, "%s", &GPIO_txt[2][0]);
    return 0;
  case Setting::RTL_GPIO_D_PIN_EN:
    snprintf(description, 1024, "%s", "GPIO Button D: Pin Number (0 .. 7), -1 for BiasT, other values to disable");
    snprintf(value, 1024, "%d", GPIO_en[3] ? GPIO_pin[3] : (-10 - GPIO_pin[3]) );
    return 0;
  case Setting::RTL_GPIO_D_INVERT:
    snprintf(description, 1024, "%s", "GPIO Button D: Invert Pin Value?: 0 = value as CheckBox. 1 = inverted");
    snprintf(value, 1024, "%d", GPIO_inv[3]);
    return 0;
  case Setting::RTL_GPIO_D_VALUE:
    snprintf(description, 1024, "%s", "GPIO Button D: Checkbox Value?: 0 = unchecked. 1 = checked");
    snprintf(value, 1024, "%d", new_GPIO[3]);
    return 0;
  case Setting::RTL_GPIO_D_LABEL:
    snprintf(description, 1024, "%s", "GPIO Button D: Label to be shown at CheckBox - max size 16");
    snprintf(value, 1024, "%s", &GPIO_txt[3][0]);
    return 0;
  case Setting::RTL_GPIO_E_PIN_EN:
    snprintf(description, 1024, "%s", "GPIO Button E: Pin Number (0 .. 7), -1 for BiasT, other values to disable");
    snprintf(value, 1024, "%d", GPIO_en[4] ? GPIO_pin[4] : (-10 - GPIO_pin[4]) );
    return 0;
  case Setting::RTL_GPIO_E_INVERT:
    snprintf(description, 1024, "%s", "GPIO Button E: Invert Pin Value?: 0 = value as CheckBox. 1 = inverted");
    snprintf(value, 1024, "%d", GPIO_inv[4]);
    return 0;
  case Setting::RTL_GPIO_E_VALUE:
    snprintf(description, 1024, "%s", "GPIO Button E: Checkbox Value?: 0 = unchecked. 1 = checked");
    snprintf(value, 1024, "%d", new_GPIO[4]);
    return 0;
  case Setting::RTL_GPIO_E_LABEL:
    snprintf(description, 1024, "%s", "GPIO Button E: Label to be shown at CheckBox - max size 16");
    snprintf(value, 1024, "%s", &GPIO_txt[4][0]);
    return 0;

  default:
    return -1;	// ERROR
  }
	return -1;	// ERROR
}

extern "C"
void  LIBRTL_API __stdcall ExtIoSetSetting( int idx, const char * value )
{
  static bool bCompatibleSettings = true;  // initial assumption
  int tempInt;

  init_toml_config();     // process as early as possible, but that depends on SDR software

  // in case settings are not compatible: keep internal defaults
  if ( !bCompatibleSettings )
    return;

  switch ( Setting(idx) )
  {
  case Setting::ID:
    bCompatibleSettings = ( 0 == strcmp(SETTINGS_IDENTIFIER, value) );
    return;
  case Setting::RTL_TCP_IP:
    snprintf(RTL_TCP_IPAddr, 31, "%s", value);
    return;
  case Setting::RTL_TCP_PORTNO:
    tempInt = atoi(value);
    if (tempInt >= 0 && tempInt < 65536)
      RTL_TCP_PortNo = tempInt;
    return;
  case Setting::AUTO_RECONNECT:
    AutoReConnect = atoi(value) ? 1 : 0;
    return;
  case Setting::PERSISTENT_CONN:
    PersistentConnection = atoi(value) ? 1 : 0;
    return;
  case Setting::SRATE_IDX:
    tempInt = atoi( value );
    if (tempInt >= 0 && tempInt < n_srates)
      new_srate_idx = tempInt;
    return;
  case Setting::TUNER_BW:
    new_TunerBW = atoi(value);
    return;
  case Setting::TUNER_RF_AGC:
    new_TunerRF_AGC = atoi(value) ? 1 : 0;
    return;
  case Setting::RTL_DIG_AGC:
    new_RTLAGC = atoi(value) ? 1 : 0;
    return;
  case Setting::CALIB_PPM:
    tempInt = atoi( value );
    if (  tempInt>MIN_PPM && tempInt < MAX_PPM )
      new_FreqCorrPPM = tempInt;
    return;
  case Setting::MANUAL_RF_GAIN:
    new_rf_gain = atoi(value);
    return;
  case Setting::BUFFER_SIZE_IDX:
    tempInt = atoi( value );
    if (  tempInt>=0 && tempInt < (sizeof(buffer_sizes)/sizeof(buffer_sizes[0])) )
      bufferSizeIdx = tempInt;
    return;
  case Setting::E4K_OFFSET_TUNE:
    new_OffsetTuning = atoi(value) ? 1 : 0;
    return;
  case Setting::DIRECT_SAMPLING_MODE:
    tempInt = atoi( value );
    if (tempInt < 0)	tempInt = 0;	else if (tempInt >2)	tempInt = 2;
    new_DirectSampling  = tempInt;
    break;
  case Setting::ASYNC_SOCKET_IO:
    ASyncSocketIO = atoi(value) ? 1 : 0;
    break;
  case Setting::SLEEP_MILLIS_FOR_DATA:
    SleepMillisWaitingForData = atoi(value);
    if (SleepMillisWaitingForData > 100)
      SleepMillisWaitingForData = 100;
    break;
  case Setting::DECIMATION:
    new_Decimation = atoi(value);
    if (new_Decimation < 1)
      new_Decimation = 1;
    else if (new_Decimation > 2)
      new_Decimation = new_Decimation & (~1);
    break;

  case Setting::TUNER_IF_AGC:             // int new_TunerIF_AGC = 1
    new_TunerIF_AGC = atoi(value) ? 1 : 0;
    break;
  case Setting::MANUAL_IF_GAIN_IDX:       // int new_if_gain_idx = 1
    new_if_gain_idx = atoi(value);
    break;
  case Setting::MANUAL_IF_GAIN_VAL:       // int new_if_gain = 1
    new_if_gain = atoi(value);
    break;
  case Setting::R820T_BAND_CENTER_SEL:    // int new_BandCenter_Sel = 0
    new_BandCenter_Sel = atoi(value);
    break;
  case Setting::R820T_BAND_CENTER_DELTA:  // int new_BandCenter_LO_Delta = 0
    new_BandCenter_LO_Delta =  atoi(value);
    break;
  case Setting::R820T_USB_SIDEBAND:       // int new_USB_Sideband = 0
    new_USB_Sideband = atoi(value) ? 1 : 0;
    break;

  case Setting::RTL_GPIO_A_PIN_EN:
    tempInt = atoi(value);
    GPIO_pin[0] = (tempInt >= -1) ? tempInt : (-10 - tempInt);
    GPIO_en[0] = (-1 <= GPIO_pin[0] && GPIO_pin[0] < 8) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_A_INVERT:
    GPIO_inv[0] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_A_VALUE:
    new_GPIO[0] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_A_LABEL:
    snprintf(&GPIO_txt[0][0], 15, "%s", value); GPIO_txt[0][15] = 0;
    break;

  case Setting::RTL_GPIO_B_PIN_EN:
    tempInt = atoi(value);
    GPIO_pin[1] = (tempInt >= -1) ? tempInt : (-10 - tempInt);
    GPIO_en[1] = (-1 <= GPIO_pin[1] && GPIO_pin[1] < 8) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_B_INVERT:
    GPIO_inv[1] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_B_VALUE:
    new_GPIO[1] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_B_LABEL:
    snprintf(&GPIO_txt[1][0], 15, "%s", value); GPIO_txt[1][15] = 0;
    break;

  case Setting::RTL_GPIO_C_PIN_EN:
    tempInt = atoi(value);
    GPIO_pin[2] = (tempInt >= -1) ? tempInt : (-10 - tempInt);
    GPIO_en[2] = (-1 <= GPIO_pin[2] && GPIO_pin[2] < 8) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_C_INVERT:
    GPIO_inv[2] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_C_VALUE:
    new_GPIO[2] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_C_LABEL:
    snprintf(&GPIO_txt[2][0], 15, "%s", value); GPIO_txt[2][15] = 0;
    break;

  case Setting::RTL_GPIO_D_PIN_EN:
    tempInt = atoi(value);
    GPIO_pin[3] = (tempInt >= -1) ? tempInt : (-10 - tempInt);
    GPIO_en[3] = (-1 <= GPIO_pin[3] && GPIO_pin[3] < 8) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_D_INVERT:
    GPIO_inv[3] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_D_VALUE:
    new_GPIO[3] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_D_LABEL:
    snprintf(&GPIO_txt[3][0], 15, "%s", value); GPIO_txt[3][15] = 0;
    break;

  case Setting::RTL_GPIO_E_PIN_EN:
    tempInt = atoi(value);
    GPIO_pin[4] = (tempInt >= -1) ? tempInt : (-10 - tempInt);
    GPIO_en[4] = (-1 <= GPIO_pin[4] && GPIO_pin[4] < 8) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_E_INVERT:
    GPIO_inv[4] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_E_VALUE:
    new_GPIO[4] = atoi(value) ? 1 : 0;
    break;
  case Setting::RTL_GPIO_E_LABEL:
    snprintf(&GPIO_txt[4][0], 15, "%s", value); GPIO_txt[4][15] = 0;
    break;

  }
}


extern "C"
void LIBRTL_API __stdcall StopHW()
{
	SDRLOG(extHw_MSG_DEBUG, "StopHW()");

	ThreadStreamToSDR = false;
	if (!PersistentConnection)
		Stop_Thread();

	if (h_dialog)
	{
		EnableWindow(GetDlgItem(h_dialog, IDC_IP_PORT), TRUE);
		EnableWindow(GetDlgItem(h_dialog, IDC_DIRECT), TRUE);
	}
}

extern "C"
void LIBRTL_API __stdcall CloseHW()
{
	SDRLOG(extHw_MSG_DEBUG, "CloseHW()");

	ThreadStreamToSDR = false;
	Stop_Thread();

	if (h_dialog)
		DestroyWindow(h_dialog);
}

extern "C"
void LIBRTL_API __stdcall ShowGUI()
{
	if (h_dialog)
	{
		ShowWindow(h_dialog, SW_SHOW);
		SetForegroundWindow(h_dialog);
	}
}

extern "C"
void LIBRTL_API  __stdcall HideGUI()
{
	if (h_dialog)
		ShowWindow(h_dialog,SW_HIDE);
}

extern "C"
void LIBRTL_API  __stdcall SwitchGUI()
{
	if (h_dialog)
	{
		if (IsWindowVisible(h_dialog))
			ShowWindow(h_dialog, SW_HIDE);
		else
			ShowWindow(h_dialog, SW_SHOW);
	}
}


extern "C"
void LIBRTL_API __stdcall SetCallback( pfnExtIOCallback funcptr )
{
    gpfnExtIOCallbackPtr = funcptr;
}

extern "C"
void LIBRTL_API  __stdcall VersionInfo(const char * progname, int ver_major, int ver_minor)
{
	if (!strcmp(progname, "HDSDR")
		&& (ver_major >= 3 || (ver_major == 2 && ver_minor > 70)))
	{
		SDRsupportsSamplePCMU8 = true;
		SDRLOG(extHw_MSG_DEBUG, "detected HDSDR > 2.70. => enabling PCMU8");
	}
}

extern "C"
void LIBRTL_API  __stdcall ExtIoSDRInfo( int extSDRInfo, int additionalValue, void * additionalPtr )
{
    init_toml_config();     // process as early as possible, but that depends on SDR software

	if (extSDRInfo == extSDR_supports_PCMU8)
	{
		SDRsupportsSamplePCMU8 = true;
		SDRLOG(extHw_MSG_DEBUG, "detected SDR with PCMU8 capability => enabling PCMU8");
	}
	else if (extSDRInfo == extSDR_supports_Logging)
		SDRsupportsLogging = true;
	else if (extSDRInfo == extSDR_supports_SampleFormats)
		SDRsupportsSampleFormats = true;
}

int Start_Thread()
{
	//If already running, exit
	if (worker_handle != INVALID_HANDLE_VALUE)
		return 0;			// all fine

	terminateThread = false;
	GotTunerInfo = false;

	if (!rcvBufsAllocated)
	{
		short_buf = new (std::nothrow) short[MAX_BUFFER_LEN + 1024];
		if (short_buf == 0)
		{
			MessageBox(NULL, TEXT("Couldn't Allocate Sample Buffer!"), TEXT("Error!"), MB_OK | MB_ICONERROR);
			return -1;
		}
		for (int k = 0; k <= NUM_BUFFERS_BEFORE_CALLBACK; ++k)
		{
			rcvBuf[k] = new (std::nothrow) uint8_t[MAX_BUFFER_LEN + 1024];
			if (rcvBuf[k] == 0)
			{
				MessageBox(NULL, TEXT("Couldn't Allocate Sample Buffers!"), TEXT("Error!"), MB_OK | MB_ICONERROR);
				return -1;
			}
		}
		rcvBufsAllocated = true;
	}

	worker_handle = (HANDLE) _beginthread( ThreadProc, 0, NULL );
	if(worker_handle == INVALID_HANDLE_VALUE)
		return -1;	// ERROR

	//SetThreadPriority(worker_handle, THREAD_PRIORITY_TIME_CRITICAL);
	return 0;
}

int Stop_Thread()
{
	if(worker_handle == INVALID_HANDLE_VALUE)
		return 0;

	terminateThread = true;
	WaitForSingleObject(worker_handle,INFINITE);
	GotTunerInfo = false;
	return 0;
}


void ThreadProc(void *p)
{
    while (!terminateThread)
    {
        // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
        rtl_tcp_dongle_info.ui[1] = tunerNo = 0;
        rtl_tcp_dongle_info.ui[2] = numTunerGains = 0;

        GotTunerInfo = false;
        if (h_dialog)
            PostMessage(h_dialog, WM_PRINT, (WPARAM)0, (LPARAM)PRF_CLIENT);

        CActiveSocket conn;
        const bool initOK = conn.Initialize();
        const bool connOK = conn.Open(RTL_TCP_IPAddr, (uint16_t)RTL_TCP_PortNo);

        if (connOK)
        {
            SDRLOG(extHw_MSG_DEBUG, "TCP connect was successful");
            if (GUIDebugConnection)
                ::MessageBoxA(0, "TCP connect was successful", "Status", 0);
        }
        else
        {
            SDRLOG(extHw_MSG_DEBUG, "TCP connect failed! Retry ..");
            // ::MessageBoxA(0, "TCP connect failed!\nRetry ..", "Status", 0);
            goto label_reConnect;
        }

        if (ASyncSocketIO)
            conn.SetNonblocking();
        else
            conn.SetBlocking();

        // on connection server will transmit dongle_info once
        int readHdr = 0;
        bool hdrOK = true;
        while (!terminateThread)
        {
            int32 toRead = 12 - readHdr;
            int32 nRead = conn.Receive(toRead);
            if (nRead > 0)
            {
                uint8_t *nBuf = conn.GetData();
                memcpy((void*)(&rtl_tcp_dongle_info.ac[readHdr]), nBuf, nRead);
                if (readHdr < 4 && readHdr + nRead >= 4)
                {
                    if (   rtl_tcp_dongle_info.ac[0] != 'R'
                        && rtl_tcp_dongle_info.ac[1] != 'T'
                        && rtl_tcp_dongle_info.ac[2] != 'L'
                        && rtl_tcp_dongle_info.ac[3] != '0' )
                    {
                        // It has to start with "RTL0"!
                        if (SDRsupportsLogging)
                            SDRLOG(extHw_MSG_ERRDLG, "Error: Stream is not from rtl_tcp. Change Source!");
                        else
                            ::MessageBoxA(0, "Error: Stream is not from rtl_tcp", "Error", 0);
                        hdrOK = false;
                        break;
                    }
                }
                readHdr += nRead;
                if (readHdr >= 12)
                    break;
            }
            else
            {
                CSimpleSocket::CSocketError err = conn.GetSocketError();
                if (CSimpleSocket::SocketSuccess != err && CSimpleSocket::SocketEwouldblock != err)
                {
                    char acMsg[256];
                    snprintf(acMsg, 255, "Socket Error %d after %d bytes in header!", (int)err, nRead);
                    if (SDRsupportsLogging)
                        SDRLOG(extHw_MSG_ERRDLG, acMsg);
                    else
                        ::MessageBoxA(0, acMsg, "Socket Error", 0);
                    hdrOK = false;
                    break;
                }
                else if (CSimpleSocket::SocketEwouldblock == err && SleepMillisWaitingForData >= 0)
                    ::Sleep(SleepMillisWaitingForData);
            }
        }
        if (!hdrOK)
            goto label_reConnect;

        rtl_tcp_dongle_info.ui[1] = tunerNo = ntohl(rtl_tcp_dongle_info.ui[1]);
        rtl_tcp_dongle_info.ui[2] = numTunerGains = ntohl(rtl_tcp_dongle_info.ui[2]);

        // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
        if (tunerNo == 2 || tunerNo == 4)
        {
          // avoid RESET of Tuner: disable GPIO4
          for (int btnNo = 0; btnNo < NUM_GPIO_BUTTONS; ++btnNo)
          {
            if (GPIO_en[btnNo] && GPIO_pin[btnNo] == 4)
            {
              GPIO_en[btnNo] = 0;
              if (h_dialog)
              {
                HWND hDlgItmGPIObtn = GetDlgItem(h_dialog, IDC_GPIO_A + btnNo);
                //SendMessage(hDlgItmGPIObtn, WM_ENABLE, (WPARAM)(GPIO_en[btnNo] ? TRUE : FALSE), NULL);
                EnableWindow(hDlgItmGPIObtn, GPIO_en[btnNo] ? TRUE : FALSE);
              }
            }
          }
        }
        else
        {
          // re-enable GPIO4 with other Tuners
          for (int btnNo = 0; btnNo < NUM_GPIO_BUTTONS; ++btnNo)
          {
            if (!GPIO_en[btnNo] && GPIO_pin[btnNo] == 4)
            {
              GPIO_en[btnNo] = 1;
              if (h_dialog)
              {
                HWND hDlgItmGPIObtn = GetDlgItem(h_dialog, IDC_GPIO_A + btnNo);
                //SendMessage(hDlgItmGPIObtn, WM_ENABLE, (WPARAM)(GPIO_en[btnNo] ? TRUE : FALSE), NULL);
                EnableWindow(hDlgItmGPIObtn, GPIO_en[btnNo] ? TRUE : FALSE);
              }
            }
          }
        }

        GotTunerInfo = true;
        if (h_dialog)
            PostMessage(h_dialog, WM_PRINT, (WPARAM)0, (LPARAM)PRF_CLIENT);

        if (GUIDebugConnection)
            ::MessageBoxA(0, "received 12 bytes header", "Status", 0);

        // update bandwidths
        bandwidths = tuner_bws[tunerNo].bw;
        n_bandwidths = tuner_bws[tunerNo].num;
        if (n_bandwidths)
        {
            int bwIdx = nearestBwIdx(new_TunerBW);
            new_TunerBW = bandwidths[bwIdx];
            last_TunerBW = new_TunerBW + 1;
        }

        // update hf gains
        rf_gains = tuner_rf_gains[tunerNo].gain;
        n_rf_gains = tuner_rf_gains[tunerNo].num;
        if (n_rf_gains)
        {
            int gainIdx = nearestGainIdx(new_rf_gain, rf_gains, n_rf_gains);
            new_rf_gain = rf_gains[gainIdx];
            last_rf_gain = new_rf_gain + 10;
        }

        // update if gains
        if_gains = tuner_if_gains[tunerNo].gain;
        n_if_gains = tuner_if_gains[tunerNo].num;
        if (n_if_gains)
        {
            new_if_gain_idx = nearestGainIdx(new_if_gain, if_gains, n_if_gains);
            new_if_gain = if_gains[new_if_gain_idx];
            last_if_gain = new_if_gain + 10;
            last_if_gain_idx = new_if_gain_idx + 1;
        }

        char acMsg[256];
        int prevBufferIdx = NUM_BUFFERS_BEFORE_CALLBACK - 1;
        int receiveBufferIdx = 0;
        int receivedLen = 0;
        int receiveOffset = 2 * MAX_DECIMATIONS;
        unsigned receivedBlocks = 0;
        int initialSrate = 1;
        int receivedSamples = 0;
        bool printCallbackLen = true;
        commandEverything = true;
        memset(&rcvBuf[prevBufferIdx][0], 0, MAX_BUFFER_LEN + 2 * MAX_DECIMATIONS);

        while (!terminateThread)
        {
            if (ThreadStreamToSDR && (somewhat_changed || commandEverything))
            {
                if (somewhat_changed & 32768)
                {
                    commandEverything = true;
                    somewhat_changed &= ~(32768);
                }

                if (last_DirectSampling != new_DirectSampling || commandEverything)
                {
                    if (!transmitTcpCmd(conn, 0x09, new_DirectSampling))
                        break;
                    last_DirectSampling = new_DirectSampling;
                    somewhat_changed &= ~(32);
                }
                if (last_OffsetTuning != new_OffsetTuning || commandEverything)
                {
                    if (!transmitTcpCmd(conn, 0x0A, new_OffsetTuning))
                        break;
                    last_OffsetTuning = new_OffsetTuning;
                    somewhat_changed &= ~(64);
                }
                if (last_USB_Sideband != new_USB_Sideband || commandEverything)
                {
                    if (!transmitTcpCmd(conn, 0x47, new_USB_Sideband))
                        break;
                    last_USB_Sideband = new_USB_Sideband;
                    somewhat_changed &= ~(1024);
                }
                for (int btnNo = 0; btnNo < NUM_GPIO_BUTTONS; ++btnNo)
                {
                  if (GPIO_en[btnNo] && (last_GPIO[btnNo] != new_GPIO[btnNo] || commandEverything))
                  {
                    const int GPIOpin = GPIO_pin[btnNo];
                    const int GPIOval = new_GPIO[btnNo] ^ GPIO_inv[btnNo];
                    if (GPIOpin < 0)
                      transmitTcpCmd(conn, 0x0E, GPIOval); // SET_BIAS_TEE
                    else
                      transmitTcpCmd(conn, 0x52, (GPIOpin << 16) | GPIOval); // GPIO_WRITE_PIN
                    last_GPIO[btnNo] = new_GPIO[btnNo];
                  }
                  somewhat_changed &= ~(8192);
                }
                if (last_FreqCorrPPM != new_FreqCorrPPM || commandEverything)
                {
                    if (!transmitTcpCmd(conn, 0x05, new_FreqCorrPPM))
                        break;
                    last_FreqCorrPPM = new_FreqCorrPPM;
                    somewhat_changed &= ~(128);
                }
                if (last_BandCenter_Sel != new_BandCenter_Sel || commandEverything)
                {
                    int fs = samplerates[new_srate_idx].valueInt;
                    int band_center = 0;
                    if (new_BandCenter_Sel == 1)
                        band_center = fs / 4;
                    else if (new_BandCenter_Sel == 2)
                        band_center = -fs / 4;

                    if (!transmitTcpCmd(conn, 0x45, band_center))
                        break;
                    last_BandCenter_Sel = new_BandCenter_Sel;
                    last_BandCenter_LO_Delta = new_BandCenter_LO_Delta;
                    somewhat_changed &= ~(2048);
                    somewhat_changed |= 1;
                }
                if (last_LO_freq != new_LO_freq || (somewhat_changed & 1) || commandEverything)
                {
                    uint64_t f64 = (uint64_t)new_LO_freq;
                    uint32_t fhi = (uint32_t)( (f64 >> 32) & 0xffffffff );
                    uint32_t flo = (uint32_t)( f64 & 0xffffffff );
                    if (fhi == 0xffffffff)  /* compiler bug - with sign at >> 32 ? */
                        fhi = 0;
                    if (!transmitTcpCmd(conn, 0x56, fhi))  /* SET_FREQ_HI32 */
                        break;
                    if (!transmitTcpCmd(conn, 0x01, flo))   /* SET_FREQUENCY */
                        break;
                    last_LO_freq = new_LO_freq;
                    somewhat_changed &= ~(1);
                }
                if (last_srate_idx != new_srate_idx || commandEverything)
                {
                    // re-parametrize Tuner RF AGC
                    {
                        transmitTcpCmd(conn, 0x03, 1 - new_TunerRF_AGC);
                        last_TunerRF_AGC = new_TunerRF_AGC;
                        somewhat_changed &= ~(8);
                    }

                    // re-parametrize Gain
                    if (new_TunerRF_AGC == 0)
                    {
                        transmitTcpCmd(conn, 0x04, new_rf_gain);
                        last_rf_gain = new_rf_gain;
                        somewhat_changed &= ~(4);
                    }

                    // re-parametrize Tuner IF AGC and/or IF gain
                    if ( new_TunerIF_AGC )
                    {
                        transmitTcpCmd(conn, 0x46, 0);
                        last_TunerIF_AGC = new_TunerIF_AGC;
                        last_if_gain_idx = new_if_gain_idx;
                        somewhat_changed &= ~(4096);
                    }
                    else
                    {
                        transmitTcpCmd(conn, 0x46, 10000 + new_if_gain_idx );
                        last_TunerIF_AGC = new_TunerIF_AGC;
                        last_if_gain_idx = new_if_gain_idx;
                        somewhat_changed &= ~(4096);
                    }

                    // re-parametrize Tuner Bandwidth
                    {
                        if (n_bandwidths)
                        {
                            //transmitTcpCmd(conn, 0x0E, new_TunerBW * 1000);   // was 0x0E
                            transmitTcpCmd(conn, 0x40, new_TunerBW * 1000);   // was 0x0E
                        }
                        last_TunerBW = new_TunerBW;
                        somewhat_changed &= ~(256);
                    }

                    // re-parametrize samplerate
                    {
                        if (!transmitTcpCmd(conn, 0x02, samplerates[new_srate_idx].valueInt))
                            break;
                        last_srate_idx = new_srate_idx;
                        somewhat_changed &= ~(2);
                    }

                    if (last_BandCenter_Sel != new_BandCenter_Sel || commandEverything)
                    {
                        int fs = samplerates[new_srate_idx].valueInt;
                        int band_center = 0;
                        if (new_BandCenter_Sel == 1)
                            band_center = fs / 4;
                        else if (new_BandCenter_Sel == 2)
                            band_center = -fs / 4;

                        if (!transmitTcpCmd(conn, 0x45, band_center))
                            break;
                        last_BandCenter_Sel = new_BandCenter_Sel;
                        somewhat_changed &= ~(2048);
                    }
                }

                if (last_TunerBW != new_TunerBW )
                {
                    //if (!transmitTcpCmd(conn, 0x0E, new_TunerBW*1000))
                    //	break;
                    if (!transmitTcpCmd(conn, 0x40, new_TunerBW * 1000))
                        break;
                    last_TunerBW = new_TunerBW;
                    somewhat_changed &= ~(256);
                }

                if (last_TunerRF_AGC != new_TunerRF_AGC)
                {
                    if (!transmitTcpCmd(conn, 0x03, 1-new_TunerRF_AGC))
                        break;
                    last_TunerRF_AGC = new_TunerRF_AGC;
                    if (new_TunerRF_AGC == 0)
                        last_rf_gain = new_rf_gain + 1;
                    somewhat_changed &= ~(8);
                }

                if (last_RTLAGC != new_RTLAGC || commandEverything)
                {
                    if (!transmitTcpCmd(conn, 0x08, new_RTLAGC))
                        break;
                    last_RTLAGC = new_RTLAGC;
                    somewhat_changed &= ~(16);
                }

                if (last_rf_gain != new_rf_gain)
                {
                    if (new_TunerRF_AGC == 0)
                    {
                        // transmit manual gain only when TunerAGC is off
                        if (!transmitTcpCmd(conn, 0x04, new_rf_gain))
                            break;
                    }
                    last_rf_gain = new_rf_gain;
                    somewhat_changed &= ~(4);
                }

                if (last_TunerIF_AGC != new_TunerIF_AGC || last_if_gain_idx != new_if_gain_idx )
                {
                    if (new_TunerIF_AGC)
                    {
                        if (!transmitTcpCmd(conn, 0x46, 0))
                            break;
                        last_TunerIF_AGC = new_TunerIF_AGC;
                        last_if_gain_idx = new_if_gain_idx;
                        somewhat_changed &= ~(4096);
                    }
                    else
                    {
                        if (!transmitTcpCmd(conn, 0x46, 10000 + new_if_gain_idx))
                            break;
                        last_TunerIF_AGC = new_TunerIF_AGC;
                        last_if_gain_idx = new_if_gain_idx;
                        somewhat_changed &= ~(4096);
                    }
                }

                if (last_Decimation != new_Decimation)
                {
                    last_Decimation = new_Decimation;
                    somewhat_changed &= ~(512);
                }

                commandEverything = false;
            }

            int32 toRead = buffer_len - receivedLen;
            int32 nRead = conn.Receive(toRead, &rcvBuf[receiveBufferIdx][receiveOffset]);
            if (nRead > 0)
            {
                receivedLen += nRead;
                receiveOffset += nRead;
                if (receivedLen >= buffer_len)
                {
                    // copy last MAX_DECIMATIONS I/Q samples from end of prevBufferIdx to begin of current received buffer
                    for (int k = 1; k <= 2 * MAX_DECIMATIONS; ++k)
                    	rcvBuf[receiveBufferIdx][2 * MAX_DECIMATIONS - k] = rcvBuf[prevBufferIdx][2 * MAX_DECIMATIONS + buffer_len - k];
                    prevBufferIdx = receiveBufferIdx;
                    
                    if (!ThreadStreamToSDR)
                    {
                    	commandEverything = true;
                    	receiveBufferIdx = 0;			// restart reception with 1st decimation buffer
                    }
                    else
                    {
                        ++receiveBufferIdx;
                        if (receiveBufferIdx >= new_Decimation)
                        {
                            // start over with 1st decimation block - for next reception
                            receiveBufferIdx = 0;
                            const int n_samples_per_block = buffer_len / 2;
                            const int n_output_per_block = n_samples_per_block / new_Decimation;
                            short * short_ptr = &short_buf[0];
                            for (int callbackBufferNo = 0; callbackBufferNo < new_Decimation; ++callbackBufferNo)
                            {
                                if (new_Decimation > 1 && extHWtype == exthwUSBdata16 )
                                {
                                    const unsigned char* char_ptr = &rcvBuf[callbackBufferNo][2*MAX_DECIMATIONS - 2*new_Decimation];
#if ( !FULL_DECIMATION )
                                    // block always starts from scratch without decimation
                                    short_ptr = &short_buf[0];
#endif
                                    int i;

#if ( FULL_DECIMATION )
	#define CHAR_PTR_INC(D)		(2 * D);
	#define ITER_COUNT			n_output_per_block
#else
	#define CHAR_PTR_INC(D)		(2);
	#define ITER_COUNT			n_samples_per_block
#endif
                                    switch (new_Decimation)
                                    {
                                    case 1:
                                        for (i = 0; i < ITER_COUNT; i++)
                                        {
                                            *short_ptr++ = ((short)(char_ptr[0])) - 128;
                                            *short_ptr++ = ((short)(char_ptr[1])) - 128;
                                            char_ptr += 2;
                                        }
                                        break;
                                    case 2:
                                        for (i = 0; i < ITER_COUNT; i++)
                                        {
                                            *short_ptr++ = ((short)(char_ptr[0])) + ((short)(char_ptr[2])) - 2 * 128;
                                            *short_ptr++ = ((short)(char_ptr[1])) + ((short)(char_ptr[3])) - 2 * 128;
                                            char_ptr += CHAR_PTR_INC(2);
                                        }
                                        break;
                                    case 4:
                                        for (i = 0; i < ITER_COUNT; i++)
                                        {
                                            *short_ptr++ = ((short)(char_ptr[0]))
                                                + ((short)(char_ptr[2]))
                                                + ((short)(char_ptr[4]))
                                                + ((short)(char_ptr[6])) - 4 * 128;
                                            *short_ptr++ = ((short)(char_ptr[1]))
                                                + ((short)(char_ptr[3]))
                                                + ((short)(char_ptr[5]))
                                                + ((short)(char_ptr[7])) - 4 * 128;
                                            char_ptr += CHAR_PTR_INC(4);
                                        }
                                        break;
                                    case 6:
                                        for (i = 0; i < ITER_COUNT; i++)
                                        {
                                            *short_ptr++ = ((short)(char_ptr[0]))
                                                + ((short)(char_ptr[2]))
                                                + ((short)(char_ptr[4]))
                                                + ((short)(char_ptr[6]))
                                                + ((short)(char_ptr[8]))
                                                + ((short)(char_ptr[10])) - 6 * 128;
                                            *short_ptr++ = ((short)(char_ptr[1]))
                                                + ((short)(char_ptr[3]))
                                                + ((short)(char_ptr[5]))
                                                + ((short)(char_ptr[7]))
                                                + ((short)(char_ptr[9]))
                                                + ((short)(char_ptr[11])) - 6 * 128;
                                            char_ptr += CHAR_PTR_INC(6);
                                        }
                                        break;
                                    case 8:
                                        for (i = 0; i < ITER_COUNT; i++)
                                        {
                                            *short_ptr++ = ((short)(char_ptr[0]))
                                                + ((short)(char_ptr[2]))
                                                + ((short)(char_ptr[4]))
                                                + ((short)(char_ptr[6]))
                                                + ((short)(char_ptr[8]))
                                                + ((short)(char_ptr[10]))
                                                + ((short)(char_ptr[12]))
                                                + ((short)(char_ptr[14])) - 8 * 128;
                                            *short_ptr++ = ((short)(char_ptr[1]))
                                                + ((short)(char_ptr[3]))
                                                + ((short)(char_ptr[5]))
                                                + ((short)(char_ptr[7]))
                                                + ((short)(char_ptr[9]))
                                                + ((short)(char_ptr[11]))
                                                + ((short)(char_ptr[13]))
                                                + ((short)(char_ptr[15])) - 8 * 128;
                                            char_ptr += CHAR_PTR_INC(8);
                                        }
                                        break;
                                    }
#if ( !FULL_DECIMATION )
                                    if (printCallbackLen)
                                    {
                                        printCallbackLen = false;
                                        snprintf(acMsg, 255, "Callback() with %d non-decimated I/Q pairs", ITER_COUNT);
                                        SDRLOG(extHw_MSG_DEBUG, acMsg);
                                    }
                                    gpfnExtIOCallbackPtr(ITER_COUNT, 0, 0, short_buf);
#endif
                                }
                                else
                                {
                                    if (extHWtype == exthwUSBdata16)
                                    {
                                        short *short_ptr = &short_buf[0];
                                        const unsigned char* char_ptr = &rcvBuf[callbackBufferNo][2*MAX_DECIMATIONS];
                                        for (int i = 0; i < buffer_len; i++)
                                            *short_ptr++ = ((short)(*char_ptr++)) - 128;
                                        if (printCallbackLen)
                                        {
                                            printCallbackLen = false;
                                            snprintf(acMsg, 255, "Callback() with %d raw 16 bit I/Q pairs", n_samples_per_block);
                                            SDRLOG(extHw_MSG_DEBUG, acMsg);
                                        }
                                        gpfnExtIOCallbackPtr(n_samples_per_block, 0, 0, short_buf);
                                    }
                                    else
                                    {
                                        unsigned char* char_ptr = &rcvBuf[callbackBufferNo][2*MAX_DECIMATIONS];
                                        if (printCallbackLen)
                                        {
                                            printCallbackLen = false;
                                            snprintf(acMsg, 255, "Callback() with %d raw 8 Bit I/Q pairs", n_samples_per_block);
                                            SDRLOG(extHw_MSG_DEBUG, acMsg);
                                        }
                                        gpfnExtIOCallbackPtr(n_samples_per_block, 0, 0, char_ptr);
                                    }
                                }
                            } // end for

#if ( FULL_DECIMATION )
                            if (new_Decimation > 1 && extHWtype == exthwUSBdata16)
                            {
                                if (printCallbackLen)
                                {
                                    printCallbackLen = false;
                                    snprintf(acMsg, 255, "Callback() with %d decimated I/Q pairs", n_samples_per_block);
                                    SDRLOG(extHw_MSG_DEBUG, acMsg);
                                }
                                gpfnExtIOCallbackPtr(n_samples_per_block, 0, 0, short_buf);
                            }
#endif

                            if (retune_freq)
                            {
                              retune_counter -= n_samples_per_block;
                              if (retune_counter <= 0)
                              {
                                new_tuneFreq = retune_value;
                                retune_freq = false;
                                EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_TUNE);
                              }
                            }

                        }
                    }

                    ++receivedBlocks;	// network statistics

                    // prepare next receive offset / length
                    receivedLen -= buffer_len;
                    receiveOffset -= buffer_len;
                    if (receivedLen > 0)
                    {
                        snprintf(acMsg, 255, "receivedLen - buffer_len = %d != 0", receivedLen);
                        SDRLOG(extHw_MSG_DEBUG, acMsg);
                        memcpy(&rcvBuf[receiveBufferIdx][2 * MAX_DECIMATIONS], &rcvBuf[prevBufferIdx][2 * MAX_DECIMATIONS + buffer_len], receivedLen);
                    }
                }
            }
            else
            {
                CSimpleSocket::CSocketError err = conn.GetSocketError();
                if (CSimpleSocket::SocketSuccess != err && CSimpleSocket::SocketEwouldblock != err)
                {
                    char acMsg[256];
                    if (GUIDebugConnection)
                        snprintf(acMsg, 255, "Socket Error %d after %d bytes in data after %u blocks!", (int)err, receivedLen, receivedBlocks);
                    else
                        snprintf(acMsg, 255, "Socket Error %d !", (int)err);
                    
                    if (SDRsupportsLogging)
                        SDRLOG(extHw_MSG_ERRDLG, acMsg);
                    else
                        ::MessageBoxA(0, acMsg, "Socket Error", 0);
                    goto label_reConnect;
                }
                else if (CSimpleSocket::SocketEwouldblock == err && SleepMillisWaitingForData >= 0)
                    ::Sleep(SleepMillisWaitingForData);
            }
        }

label_reConnect:
        conn.Close();
        if (!AutoReConnect)
            break;
    }

    worker_handle = INVALID_HANDLE_VALUE;
    _endthread();
}


static void updateTunerBWs(HWND hwndDlg)
{
	TCHAR str[256];
	HWND hDlgItmTunerBW = GetDlgItem(hwndDlg, IDC_TUNERBANDWIDTH);

	bandwidths = tuner_bws[tunerNo].bw;
	n_bandwidths = tuner_bws[tunerNo].num;

	ComboBox_ResetContent(hDlgItmTunerBW);
	if (n_bandwidths)
		ComboBox_AddString(hDlgItmTunerBW, TEXT("Automatic") );
	for (int i = 1; i < n_bandwidths; i++)
	{
		_stprintf_s(str, 255, TEXT("~ %d kHz%s"), bandwidths[i], ( (bandwidths[i] * 1000 > MAXRATE) ? " !" : "" ) );
		ComboBox_AddString(hDlgItmTunerBW, str);
	}
	ComboBox_SetCurSel(hDlgItmTunerBW, nearestBwIdx(new_TunerBW));
}

static void updateRFTunerGains(HWND hwndDlg)
{
  HWND hRFGain = GetDlgItem(hwndDlg, IDC_RF_GAIN_SLIDER);
  HWND hGainLabel = GetDlgItem(hwndDlg, IDC_TUNER_RF_GAIN_VALUE);
	HWND hTunerBwLabel = GetDlgItem(hwndDlg, IDC_TUNER_BW_LABEL);

	rf_gains = tuner_rf_gains[tunerNo].gain;
	n_rf_gains = tuner_rf_gains[tunerNo].num;

	if (0 == tunerNo)
		Static_SetText(hTunerBwLabel, TEXT("Tuner Bandwidth:"));
	else if (0 == n_rf_gains)
	{
		TCHAR str[255];
		_stprintf_s(str, 255, TEXT("[ %s-Tuner Bandwidth ]"), TunerName[tunerNo]);
		Static_SetText(hTunerBwLabel, str);
	}
	else
	{
		TCHAR str[255];
		_stprintf_s(str, 255, TEXT("%s-Tuner Bandwidth"), TunerName[tunerNo]);
		Static_SetText(hTunerBwLabel, str);
	}

	if (n_rf_gains > 0)
	{
    SendMessage(hRFGain, TBM_SETRANGEMIN, (WPARAM)TRUE, (LPARAM)-rf_gains[n_rf_gains - 1]);
    SendMessage(hRFGain, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)-rf_gains[0]);
	}
	else
	{
    SendMessage(hRFGain, TBM_SETRANGEMIN, (WPARAM)TRUE, (LPARAM)-100);
    SendMessage(hRFGain, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)0);
	}

  SendMessage(hRFGain, TBM_CLEARTICS, (WPARAM)FALSE, (LPARAM)0);
	if (n_rf_gains > 0)
	{
		for (int i = 0; i<n_rf_gains; i++)
      SendMessage(hRFGain, TBM_SETTIC, (WPARAM)0, (LPARAM)-rf_gains[i]);

    int gainIdx = nearestGainIdx(new_rf_gain, rf_gains, n_rf_gains);
    if (new_rf_gain != rf_gains[gainIdx])
		{
      new_rf_gain = rf_gains[gainIdx];
			somewhat_changed |= 4;
		}
    SendMessage(hRFGain, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-new_rf_gain);
	}

	if (new_TunerRF_AGC)
	{
    EnableWindow(hRFGain, FALSE);
		Static_SetText(hGainLabel, TEXT("AGC"));
	}
	else
	{
    EnableWindow(hRFGain, TRUE);
    int pos = -SendMessage(hRFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
		TCHAR str[255];
		_stprintf_s(str, 255, TEXT("%2.1f dB"), (float)pos / 10);
		Static_SetText(hGainLabel, str);
	}
}


static void updateIFTunerGains(HWND hwndDlg)
{
  HWND hIFGain = GetDlgItem(hwndDlg, IDC_IF_GAIN_SLIDER);
  HWND hGainLabel = GetDlgItem(hwndDlg, IDC_TUNER_IF_GAIN_VALUE);

	if_gains = tuner_if_gains[tunerNo].gain;
	n_if_gains = tuner_if_gains[tunerNo].num;

	if (n_if_gains > 0)
	{
    SendMessage(hIFGain, TBM_SETRANGEMIN, (WPARAM)TRUE, (LPARAM)-if_gains[n_if_gains - 1]);
    SendMessage(hIFGain, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)-if_gains[0]);
	}
	else
	{
    SendMessage(hIFGain, TBM_SETRANGEMIN, (WPARAM)TRUE, (LPARAM)-100);
    SendMessage(hIFGain, TBM_SETRANGEMAX, (WPARAM)TRUE, (LPARAM)0);
	}

  SendMessage(hIFGain, TBM_CLEARTICS, (WPARAM)FALSE, (LPARAM)0);
	if (n_if_gains > 0)
	{
		for (int i = 0; i<n_if_gains; i++)
      SendMessage(hIFGain, TBM_SETTIC, (WPARAM)0, (LPARAM)-if_gains[i]);

    int gain_idx = nearestGainIdx(new_if_gain, if_gains, n_if_gains);
    if (new_if_gain != if_gains[gain_idx])
    {
      new_if_gain_idx = gain_idx;
      new_if_gain = if_gains[new_if_gain_idx];
      somewhat_changed |= 4096;
    }
    SendMessage(hIFGain, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-new_if_gain);
	}

	if (new_TunerIF_AGC)
	{
    EnableWindow(hIFGain, FALSE);
		Static_SetText(hGainLabel, TEXT("AGC"));
	}
	else
	{
    EnableWindow(hIFGain, TRUE);
    int pos = -SendMessage(hIFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
		TCHAR str[255];
		_stprintf_s(str, 255, TEXT("%2.1f dB"), (float)pos / 10);
		Static_SetText(hGainLabel, str);
	}
}



static void updateDecimations(HWND hwndDlg)
{
	TCHAR str[256];
	HWND hDecimation = GetDlgItem(hwndDlg, IDC_DECIMATION);

	maxDecimation = 1;
	//int bwIdx = nearestBwIdx(new_TunerBW);

	//_stprintf_s(str, 255, TEXT("tunerBW = %d"), new_TunerBW);
	//::MessageBoxA(NULL, str, "info", 0);

	ComboBox_ResetContent(hDecimation);
	for (int i = 1; i <= MAX_DECIMATIONS; i++)
	{
		if (i == 1)
		{
			_stprintf_s(str, 255, TEXT("/ %d"), i);
			ComboBox_AddString(hDecimation, str);
			maxDecimation = i;
		}
		else if ( (i & 1) == 0 && samplerates[new_srate_idx].valueInt >= i * 1000 * new_TunerBW * 24 / 35)
		{
			double newSrateKHz = (double)samplerates[new_srate_idx].valueInt / ( i * 1000 );
			_stprintf_s(str, 255, TEXT("/ %d  -> %.1f kHz"), i, newSrateKHz);
			ComboBox_AddString(hDecimation, str);
			maxDecimation = i;
		}
	}
	if (new_Decimation < 1)
		new_Decimation = 1;
	if (new_Decimation > maxDecimation)
		new_Decimation = maxDecimation;

	//_stprintf_s(str, 255, TEXT("maxDec %d, newDec %d"), maxDecimation, new_Decimation);
	//::MessageBoxA(NULL, str, "info", 0);

	// idx , decimation
	// 0, 1
	// 1, 2
	// 2, 4
	// 3, 6
	// 4, 8
	int decimationIdx = new_Decimation >> 1;
	ComboBox_SetCurSel(hDecimation, decimationIdx);

	if (MAX_DECIMATIONS == 1)
		EnableWindow(hDecimation, FALSE);
}


static INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH BRUSH_RED=CreateSolidBrush(RGB(255,0,0));
    static HBRUSH BRUSH_GREEN=CreateSolidBrush(RGB(0,255,0));

    switch (uMsg)
    {
        case (WM_USER + 42):
        case WM_INITDIALOG:
        {
            HWND hDlgItmOffset = GetDlgItem(hwndDlg, IDC_OFFSET);
            HWND hDlgItmUsbSB = GetDlgItem(hwndDlg, IDC_USB_SIDEBAND);
            HWND hDlgItmBandText = GetDlgItem(hwndDlg, IDC_BAND_NAME);

            if (band_disp_text[0])
                Static_SetText(hDlgItmBandText, band_disp_text);

            for (int i=0; i<(sizeof(directS)/sizeof(directS[0]));i++)
                ComboBox_AddString(GetDlgItem(hwndDlg,IDC_DIRECT),directS[i]);
            ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_DIRECT), new_DirectSampling);

            ComboBox_AddString(GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL), TEXT("DC / Center: 0"));
            ComboBox_AddString(GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL), TEXT("Upper Half: + Samplerate/4"));
            ComboBox_AddString(GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL), TEXT("Lower Half: - Samplerate/4"));
            ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL), new_BandCenter_Sel);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_AUTORECONNECT), AutoReConnect ? BST_CHECKED : BST_UNCHECKED);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_PERSISTCONNECTION), PersistentConnection ? BST_CHECKED : BST_UNCHECKED);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TUNER_RF_AGC), new_TunerRF_AGC ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TUNER_IF_AGC), new_TunerIF_AGC ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_RTL_DIG_AGC), new_RTLAGC ? BST_CHECKED : BST_UNCHECKED);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_OFFSET), new_OffsetTuning ? BST_CHECKED : BST_UNCHECKED);

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_USB_SIDEBAND), new_USB_Sideband ? BST_CHECKED : BST_UNCHECKED);

            TCHAR tempStr[256];

            for (int btnNo = 0; btnNo < NUM_GPIO_BUTTONS; ++btnNo)
            {
              HWND hDlgItmGPIObtn = GetDlgItem(hwndDlg, IDC_GPIO_A + btnNo);

              _stprintf_s(tempStr, 255, TEXT("%s"), &GPIO_txt[btnNo][0]);
              Edit_SetText(hDlgItmGPIObtn, tempStr);

              if (GPIO_en[btnNo])
              {
                EnableWindow(hDlgItmGPIObtn, TRUE);
                Button_SetCheck(hDlgItmGPIObtn, new_GPIO[btnNo] ? BST_CHECKED : BST_UNCHECKED);
              }
              else
              {
                EnableWindow(hDlgItmGPIObtn, FALSE);
                Button_SetCheck(hDlgItmGPIObtn, BST_UNCHECKED);
              }
            }

            SendMessage(GetDlgItem(hwndDlg,IDC_PPM_S), UDM_SETRANGE  , (WPARAM)TRUE, (LPARAM)MAX_PPM | (MIN_PPM << 16));

            _stprintf_s(tempStr, 255, TEXT("%d"), new_FreqCorrPPM);
            Edit_SetText(GetDlgItem(hwndDlg,IDC_PPM), tempStr );
            //rtlsdr_set_freq_correction(dev, new_FreqCorrPPM);

            {
                TCHAR tempStr[255];
                _stprintf_s(tempStr, 255, TEXT("%s:%d"), RTL_TCP_IPAddr, RTL_TCP_PortNo);
                Edit_SetText(GetDlgItem(hwndDlg, IDC_IP_PORT), tempStr);
            }

            for (int i = 0; i<n_srates; i++)
                ComboBox_AddString(GetDlgItem(hwndDlg,IDC_SAMPLERATE),samplerates[i].name);
            ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_SAMPLERATE), new_srate_idx);

            {
                for (int i = 0; i < (sizeof(buffer_sizes) / sizeof(buffer_sizes[0])); i++)
                {
                    TCHAR str[255];
                    _stprintf_s(str, 255, TEXT("%d kB"), buffer_sizes[i]);
                    ComboBox_AddString(GetDlgItem(hwndDlg, IDC_BUFFER), str);
                }
                ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_BUFFER), bufferSizeIdx);
                buffer_len = buffer_sizes[bufferSizeIdx] * 1024;
            }

            updateTunerBWs(hwndDlg);
            updateRFTunerGains(hwndDlg);
            updateIFTunerGains(hwndDlg);
            updateDecimations(hwndDlg);

            return TRUE;
        }

        case WM_PRINT:
            if (lParam == (LPARAM)PRF_CLIENT)
            {
                HWND hDlgItmOffset = GetDlgItem(hwndDlg, IDC_OFFSET);
                HWND hDlgItmUsbSB = GetDlgItem(hwndDlg, IDC_USB_SIDEBAND);
                HWND hDlgItmTunerBW = GetDlgItem(hwndDlg, IDC_TUNERBANDWIDTH);
                HWND hDlgItmBandCenter = GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL);
                HWND hDlgItmIF_Slider = GetDlgItem(hwndDlg, IDC_IF_GAIN_SLIDER);
                HWND hDlgItmIF_AGC = GetDlgItem(hwndDlg, IDC_TUNER_IF_AGC);

                updateTunerBWs(hwndDlg);
                updateRFTunerGains(hwndDlg);
                updateIFTunerGains(hwndDlg);
                updateDecimations(hwndDlg);
            
                BOOL enableOffset = (1 == tunerNo) ? TRUE : FALSE;
                BOOL enableSideBand = (5 == tunerNo) ? TRUE : FALSE;
                BOOL enableBandCenter = (5 == tunerNo) ? TRUE : FALSE;
                BOOL enableIFGain = (5 == tunerNo) ? TRUE : FALSE;
                BOOL enableTunerBW = (bandwidths && 0 == new_DirectSampling) ? TRUE : FALSE;

                EnableWindow(hDlgItmOffset, enableOffset);
                EnableWindow(hDlgItmUsbSB, enableSideBand);
                EnableWindow(hDlgItmTunerBW, enableTunerBW);
                EnableWindow(hDlgItmBandCenter, enableBandCenter);
                EnableWindow(hDlgItmIF_Slider, enableIFGain);
                EnableWindow(hDlgItmIF_AGC, enableIFGain);

                const char * tunerText = (tunerNo >= 0 && tunerNo < n_tuners)
                    ? TunerName[tunerNo] : "Tuner?";
                TCHAR str[255];
                _stprintf_s(str,255, TEXT("%s AGC"), tunerText ); 
                Static_SetText(GetDlgItem(hwndDlg, IDC_TUNER_RF_AGC), str);
            }
            return TRUE;

        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_PPM:
                if(GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE)
                {
                    TCHAR ppm[255];
                    Edit_GetText((HWND) lParam, ppm, 255 );
                    new_FreqCorrPPM = _ttoi(ppm);
                    somewhat_changed |= 128;
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_LO);
                }
                return TRUE;
            case IDC_RTL_DIG_AGC:
                {
                    new_RTLAGC = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    somewhat_changed |= 16;
                    return TRUE;
                }
            case IDC_OFFSET:
                {
                    HWND hDlgItmOffset = GetDlgItem(hwndDlg, IDC_OFFSET);

                    new_OffsetTuning = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    somewhat_changed |= 64;

                    // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
                    EnableWindow(hDlgItmOffset, (1 == rtl_tcp_dongle_info.ui[1]) ? TRUE : FALSE);

                    return TRUE;
                }
            case IDC_USB_SIDEBAND:
                {
                    HWND hDlgItmUsbSB = GetDlgItem(hwndDlg, IDC_USB_SIDEBAND);

                    new_USB_Sideband = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    somewhat_changed |= 64;

                    // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
                    EnableWindow(hDlgItmUsbSB, (5 == rtl_tcp_dongle_info.ui[1]) ? TRUE : FALSE);

                    return TRUE;
                }
            case IDC_GPIO_A:
            case IDC_GPIO_B:
            case IDC_GPIO_C:
            case IDC_GPIO_D:
            case IDC_GPIO_E:
                {
                  const int btnNo = GET_WM_COMMAND_ID(wParam, lParam) - IDC_GPIO_A;
                  HWND hDlgItmGPIObtn = GetDlgItem(hwndDlg, IDC_GPIO_A + btnNo);
                  if (GPIO_en[btnNo])
                  {
                    new_GPIO[btnNo] = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    somewhat_changed |= 8192;
                  }
                  EnableWindow(hDlgItmGPIObtn, GPIO_en[btnNo] ? TRUE : FALSE);
                  return TRUE;
                }

            case IDC_TUNER_RF_AGC:
                {
                    HWND hRFGain = GetDlgItem(hwndDlg, IDC_RF_GAIN_SLIDER);
                    HWND hGainLabel = GetDlgItem(hwndDlg, IDC_TUNER_RF_GAIN_VALUE);
                    if(Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) //it is checked
                    {
                        new_TunerRF_AGC = 1;	// automatic
                        somewhat_changed |= 8;
                        EnableWindow(hRFGain, FALSE);
                        Static_SetText(hGainLabel, TEXT("HF AGC"));
                    }
                    else //it has been unchecked
                    {
                        //rtlsdr_set_tuner_gain_mode(dev,1);
                        new_TunerRF_AGC = 0;	// manual
                        somewhat_changed |= 8;
                        EnableWindow(hRFGain, TRUE);
                        int pos = -SendMessage(hRFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
                        TCHAR str[255];
                        _stprintf_s(str,255, TEXT("%2.1f dB"),(float) pos/10); 
                        Static_SetText(hGainLabel, str);
                    }
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_RF_IF);  // Signal application
                    return TRUE;
                }

            case IDC_TUNER_IF_AGC:
                {
                    HWND hIFGain = GetDlgItem(hwndDlg, IDC_IF_GAIN_SLIDER);
                    HWND hGainLabel = GetDlgItem(hwndDlg, IDC_TUNER_IF_GAIN_VALUE);
                    if(Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) //it is checked
                    {
                        new_TunerIF_AGC = 1;	// automatic
                        last_if_gain = new_if_gain + 1;
                        last_if_gain_idx = new_if_gain_idx + 1;
                        somewhat_changed |= 4096;
                        EnableWindow(hIFGain, FALSE);
                        Static_SetText(hGainLabel, TEXT("IF AGC"));
                    }
                    else //it has been unchecked
                    {
                        new_TunerIF_AGC = 0;	// manual
                        last_if_gain = new_if_gain + 1;
                        last_if_gain_idx = new_if_gain_idx + 1;
                        somewhat_changed |= 4096;
                        // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
                        EnableWindow(hIFGain, (5 == rtl_tcp_dongle_info.ui[1]) ? TRUE : FALSE);
                        int pos = -SendMessage(hIFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
                        TCHAR str[255];
                        _stprintf_s(str,255, TEXT("%2.1f dB"),(float) pos/10); 
                        Static_SetText(hGainLabel, str);
                    }

                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_RF_IF);  // Signal application
                    return TRUE;
                }

            case IDC_AUTORECONNECT:
                {
                    AutoReConnect = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    return TRUE;
                }
            case IDC_PERSISTCONNECTION:
                {
                    PersistentConnection = (Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED) ? 1 : 0;
                    if (!PersistentConnection && !ThreadStreamToSDR)
                        Stop_Thread();
                    return TRUE;
                }
            case IDC_SAMPLERATE:
                if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    new_srate_idx = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    somewhat_changed |= 2;
                    updateDecimations(hwndDlg);
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SampleRate);  // Signal application
                }
                if (GET_WM_COMMAND_CMD(wParam, lParam) ==  CBN_EDITUPDATE)
                {
                    TCHAR  ListItem[256];
                    ComboBox_GetText((HWND) lParam,ListItem,256);
                    TCHAR *endptr;
                    double coeff = _tcstod(ListItem, &endptr);
                    while (_istspace(*endptr)) ++endptr;

                    int exp = 1;
                    switch (_totupper(*endptr)) {
                        case 'K': exp = 1000; break;
                        case 'M': exp = 1000*1000; break;
                    }

                    uint32_t newrate = uint32_t( coeff * exp );
                    if (newrate>=MINRATE && newrate<=MAXRATE) {
                        //rtlsdr_set_sample_rate(dev, newrate);
// @TODO!
                      EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SampleRate);  // Signal application
                    }

                }
                return TRUE;

            case IDC_BUFFER:
                if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    bufferSizeIdx = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    buffer_len = buffer_sizes[bufferSizeIdx] * 1024;
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SampleRate);  // Signal application
                    if (SDRsupportsLogging)
                        SDRLOG(extHw_MSG_ERRDLG, "Restart SDR application,\nthat changed buffer size has effect!");
                    else
                        ::MessageBoxA(NULL, "Restart SDR application,\nthat changed buffer size has effect!", "Info", 0);
                }
                return TRUE;

            case IDC_TUNERBANDWIDTH:
                if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    int bwIdx = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    new_TunerBW = bandwidths[bwIdx];
                    somewhat_changed |= 256;
                    updateDecimations(hwndDlg);
                }
                return TRUE;

            case IDC_DECIMATION:
                if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    // idx , decimation
                    // 0, 1
                    // 1, 2
                    // 2, 4
                    // 3, 6
                    // 4, 8
                    int idx = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    new_Decimation = (idx == 0) ? 1 : (2 * idx);
                    somewhat_changed |= 512;
#if ( ALWAYS_PCMU8 == 0 && ALWAYS_PCM16 == 0 )
                    if (SDRsupportsSamplePCMU8 && SDRsupportsSampleFormats)
                    {
                        if (new_Decimation == 1)
                        {
                            extHWtype = exthwUSBdataU8;   // 8bit samples are sufficient when not using decimation
                            EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_SampleFormat_PCMU8);
                        }
                        else
                        {
                            extHWtype = exthwUSBdata16;   // with decimation 16-bit samples are necessary
                            EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_SampleFormat_PCM16);
                        }
                    }
#endif
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SRATES);  // Signal application
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_SampleRate);  // Signal application
                }
                return TRUE;

            case IDC_DIRECT:
                if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    new_DirectSampling = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    somewhat_changed |= 32;

                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_LO);  // Signal application
                }
                return TRUE;

            case IDC_BAND_CENTER_SEL:
                if(GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
                {
                    const int64_t old_tuneFreq = new_tuneFreq;
                    int64_t tmp_LO_freq = new_LO_freq;
                    const int fs = samplerates[new_srate_idx].valueInt;
                    new_BandCenter_Sel = ComboBox_GetCurSel(GET_WM_COMMAND_HWND(wParam, lParam));
                    int band_center = 0;
                    if (new_BandCenter_Sel == 1)
                        band_center = fs / 4;
                    else if (new_BandCenter_Sel == 2)
                        band_center = -fs / 4;
                    new_BandCenter_LO_Delta = band_center;

                    if (last_BandCenter_Sel == 1)  // -> +fs/4
                      tmp_LO_freq += fs / 4;
                    else if (last_BandCenter_Sel == 2)  // -> -fs/4
                      tmp_LO_freq -= fs / 4;

                    if (new_BandCenter_Sel == 1)  // -> +fs/4
                      tmp_LO_freq -= fs / 4;
                    else if (new_BandCenter_Sel == 2)  // -> -fs/4
                      tmp_LO_freq += fs / 4;

                    new_LO_freq = tmp_LO_freq;
                    somewhat_changed |= (1 + 2048);

                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_LO);

                    // looks, we need to delay tuning back to previous frequency!
                    retune_value = old_tuneFreq;
                    retune_counter = fs / 10;
                    retune_freq = true;

                    // E4000 = 1, FC0012 = 2, FC0013 = 3, FC2580 = 4, R820T = 5, R828D = 6
                    EnableWindow( GetDlgItem(hwndDlg, IDC_BAND_CENTER_SEL), (5 == rtl_tcp_dongle_info.ui[1]) ? TRUE : FALSE);
                }
                return TRUE;

            case IDC_IP_PORT:
                if (GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE)
                {
                    TCHAR tempStr[255];
                    Edit_GetText((HWND)lParam, tempStr, 255);
                    //rtlsdr_set_freq_correction(dev, _ttoi(ppm))
                    char * IP = strtok(tempStr, ":");
                    if (IP)
                        snprintf(RTL_TCP_IPAddr, 31, "%s", IP);
                    char * PortStr = strtok(NULL, ":");
                    if (PortStr)
                    {
                        int PortNo = atoi(PortStr);
                        if (PortNo > 0 && PortNo < 65536)
                            RTL_TCP_PortNo = PortNo;
                    }
                }
                return TRUE;
            }
            break;

        case WM_VSCROLL:
        {
            HWND hRFGain = GetDlgItem(hwndDlg, IDC_RF_GAIN_SLIDER);
            if ((HWND)lParam == hRFGain)
            {
                int pos = -SendMessage(hRFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
                for (int i = 0; i < n_rf_gains - 1; ++i)
                    if (rf_gains[i] < pos && pos < rf_gains[i + 1])
                    {
                        if ((pos - rf_gains[i]) < (rf_gains[i + 1] - pos) && (LOWORD(wParam) != TB_LINEUP) || (LOWORD(wParam) == TB_LINEDOWN))
                            pos = rf_gains[i];
                        else
                            pos = rf_gains[i + 1];
                    }

                SendMessage(hRFGain, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-pos);
                TCHAR str[255];
                _stprintf_s(str, 255, TEXT("%2.1f  dB"), (float)pos / 10);
                Static_SetText(GetDlgItem(hwndDlg, IDC_TUNER_RF_GAIN_VALUE), str);

                if (pos != last_rf_gain)
                {
                    new_rf_gain = pos;
                    somewhat_changed |= 4;
                    EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_ATT);
                }
                return TRUE;
            }

            HWND hIFGain = GetDlgItem(hwndDlg, IDC_IF_GAIN_SLIDER);
            if ((HWND)lParam == hIFGain)
            {
                int pos = -SendMessage(hIFGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
                for (int i = 0; i < n_if_gains - 1; ++i)
                    if (if_gains[i] < pos && pos < if_gains[i + 1])
                    {
                        if ((pos - if_gains[i]) < (if_gains[i + 1] - pos) && (LOWORD(wParam) != TB_LINEUP) || (LOWORD(wParam) == TB_LINEDOWN))
                            pos = if_gains[i];
                        else
                            pos = if_gains[i + 1];
                    }

                SendMessage(hIFGain, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)-pos);
                TCHAR str[255];
                _stprintf_s(str, 255, TEXT("%2.1f  dB"), (float)pos / 10);
                Static_SetText(GetDlgItem(hwndDlg, IDC_TUNER_IF_GAIN_VALUE), str);

                if (pos != last_if_gain)
                {
                    new_if_gain = pos;
                    new_if_gain_idx = nearestGainIdx(new_if_gain, if_gains, n_if_gains);
                    somewhat_changed |= 4096;
                    //EXTIO_STATUS_CHANGE(gpfnExtIOCallbackPtr, extHw_Changed_ATT);
                }
                return TRUE;
            }

            if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_PPM_S))
            {
                return TRUE;
            }
        }
        break;

        case WM_CLOSE:
            ShowWindow(hwndDlg, SW_HIDE);
            return TRUE;
            break;

        case WM_DESTROY:
            h_dialog=NULL;
            return TRUE;
            break;

        /*
        * TODO: Add more messages, when needed.
        */
    }

    return FALSE;
}

