#include "stdio.h"
#include "defines.h"
#include "cast.h"
#include "radio.h"

#include <getopt.h>

#define DEFAULT_FM_DEVIATION 85000 // Gives headroom for overmodulation
#define DEFAULT_DEEMPHASIS_RATE 75 // For USA
#define DEFAULT_BB_FILTER_CUTOFF 125000
#define DEFAULT_BB_FILTER_TRANS 15000
#define DEFAULT_MPX_FILTER_CUTOFF 61000
#define DEFAULT_MPX_FILTER_TRANS 1500
#define DEFAULT_AUD_FILTER_CUTOFF 15000
#define DEFAULT_AUD_FILTER_TRANS 4000

static int frequency = 0;
static fmice_icecast* icecast_mpx = 0;
static fmice_icecast* icecast_aud = 0;
static fmice_radio_settings_t radio_settings;

int parse_freq(const char* input) {
	int p1;
	int p2;
	if (sscanf(input, "%i.%i", &p1, &p2) == 2)
		return (p1 * 1000000) + (p2 * 100000);
	return 0;
}

void help(char* pgm) {
	printf("Usage: %s\n", pgm);
	printf("    Basic Settings:\n");
	printf("        [-f Radio frequency]\n");
	printf("    MPX Icecast Settings:\n");
	printf("        [--ice-mpx-host Composite Icecast hostname]\n");
	printf("        [--ice-mpx-port Composite Icecast port]\n");
	printf("        [--ice-mpx-mount Composite Icecast mountpoint]\n");
	printf("        [--ice-mpx-user Composite Icecast username]\n");
	printf("        [--ice-mpx-pass Composite Icecast password]\n");
	printf("    Audio Icecast Settings:\n");
	printf("        [--ice-aud-host Audio Icecast hostname]\n");
	printf("        [--ice-aud-port Audio Icecast port]\n");
	printf("        [--ice-aud-mount Audio Icecast mountpoint]\n");
	printf("        [--ice-aud-user Audio Icecast username]\n");
	printf("        [--ice-aud-pass Audio Icecast password]\n");
	printf("    Advanced Settings:\n");
	printf("        [--deviation FM deviation (default is %i)]\n", DEFAULT_FM_DEVIATION);
	printf("        [--deemphasis FM deemphasis rate (default is %i - Set to 0 to disable)]\n", DEFAULT_DEEMPHASIS_RATE);
	printf("        [--bb-filter-cutoff Custom baseband filter cutoff (default is %i hz)]\n", DEFAULT_BB_FILTER_CUTOFF);
	printf("        [--bb-filter-trans Custom baseband filter transition (default is %i hz)]\n", DEFAULT_BB_FILTER_TRANS);
	printf("        [--mpx-filter-cutoff Custom composite filter cutoff (default is %i hz)]\n", DEFAULT_MPX_FILTER_CUTOFF);
	printf("        [--mpx-filter-trans Custom composite filter transition (default is %i hz)]\n", DEFAULT_MPX_FILTER_TRANS);
	printf("        [--aud-filter-cutoff Custom audio filter cutoff (default is %i hz)]\n", DEFAULT_AUD_FILTER_CUTOFF);
	printf("        [--aud-filter-trans Custom audio filter transition (default is %i hz)]\n", DEFAULT_AUD_FILTER_TRANS);
}

int parse_args(int argc, char* argv[]) {
	static const struct option long_opts[] = {
		{ "ice-mpx-host", required_argument, NULL, 11 },
		{ "ice-mpx-port", required_argument, NULL, 12 },
		{ "ice-mpx-mount", required_argument, NULL, 13 },
		{ "ice-mpx-user", required_argument, NULL, 14 },
		{ "ice-mpx-pass", required_argument, NULL, 15 },
		{ "ice-aud-host", required_argument, NULL, 21 },
		{ "ice-aud-port", required_argument, NULL, 22 },
		{ "ice-aud-mount", required_argument, NULL, 23 },
		{ "ice-aud-user", required_argument, NULL, 24 },
		{ "ice-aud-pass", required_argument, NULL, 25 },
		{ "deviation", required_argument, NULL, 31 },
		{ "deemphasis", required_argument, NULL, 32 },
		{ "bb-filter-cutoff", required_argument, NULL, 33 },
		{ "bb-filter-trans", required_argument, NULL, 34 },
		{ "mpx-filter-cutoff", required_argument, NULL, 35 },
		{ "mpx-filter-trans", required_argument, NULL, 36 },
		{ "aud-filter-cutoff", required_argument, NULL, 37 },
		{ "aud-filter-trans", required_argument, NULL, 38 },
		{ "freq", required_argument, NULL, 'f'},
		{ 0 }
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "f:", long_opts, NULL)) != -1) {
		switch (opt) {
		
		case 'f':
			frequency = parse_freq(optarg);
			break;

		case 11:
			// MPX ICECAST - HOST
			if (icecast_mpx == 0)
				icecast_mpx = new fmice_icecast(1, MPX_SAMP_RATE);
			icecast_mpx->set_host(optarg);
			break;

		case 12:
			// MPX ICECAST - PORT
			if (icecast_mpx == 0)
				icecast_mpx = new fmice_icecast(1, MPX_SAMP_RATE);
			icecast_mpx->set_port(atoi(optarg));
			break;

		case 13:
			// MPX ICECAST - MOUNT
			if (icecast_mpx == 0)
				icecast_mpx = new fmice_icecast(1, MPX_SAMP_RATE);
			icecast_mpx->set_mount(optarg);
			break;

		case 14:
			// MPX ICECAST - USER
			if (icecast_mpx == 0)
				icecast_mpx = new fmice_icecast(1, MPX_SAMP_RATE);
			icecast_mpx->set_username(optarg);
			break;

		case 15:
			// MPX ICECAST - PASS
			if (icecast_mpx == 0)
				icecast_mpx = new fmice_icecast(1, MPX_SAMP_RATE);
			icecast_mpx->set_password(optarg);
			break;

		case 21:
			// AUDIO ICECAST - HOST
			if (icecast_aud == 0)
				icecast_aud = new fmice_icecast(2, AUDIO_SAMP_RATE);
			icecast_aud->set_host(optarg);
			break;

		case 22:
			// AUDIO ICECAST - PORT
			if (icecast_aud == 0)
				icecast_aud = new fmice_icecast(2, AUDIO_SAMP_RATE);
			icecast_aud->set_port(atoi(optarg));
			break;

		case 23:
			// AUDIO ICECAST - MOUNT
			if (icecast_aud == 0)
				icecast_aud = new fmice_icecast(2, AUDIO_SAMP_RATE);
			icecast_aud->set_mount(optarg);
			break;

		case 24:
			// AUDIO ICECAST - USER
			if (icecast_aud == 0)
				icecast_aud = new fmice_icecast(2, AUDIO_SAMP_RATE);
			icecast_aud->set_username(optarg);
			break;

		case 25:
			// AUDIO ICECAST - PASS
			if (icecast_aud == 0)
				icecast_aud = new fmice_icecast(2, AUDIO_SAMP_RATE);
			icecast_aud->set_password(optarg);
			break;

		case 31:
			// DEVIATION
			radio_settings.fm_deviation = atoi(optarg);
			break;

		case 32:
			// DEEMPHASIS
			radio_settings.deemphasis_rate = atoi(optarg);
			break;

		case 33:
			// BB CUTOFF
			radio_settings.bb_filter_cutoff = atoi(optarg);
			break;

		case 34:
			// BB TRANSITION
			radio_settings.bb_filter_trans = atoi(optarg);
			break;

		case 35:
			// MPX CUTOFF
			radio_settings.mpx_filter_cutoff = atoi(optarg);
			break;

		case 36:
			// MPX TRANSITION
			radio_settings.mpx_filter_trans = atoi(optarg);
			break;

		case 37:
			// AUDIO CUTOFF
			radio_settings.aud_filter_cutoff = atoi(optarg);
			break;

		case 38:
			// AUDIO TRANSITION
			radio_settings.aud_filter_trans = atoi(optarg);
			break;

		default:
			help(argv[0]);
			return -1;
		}
	}
	return 0;
}

/// <summary>
/// Sanity checks all arguments. Returns 0 if OK, otherwise -1.
/// </summary>
/// <returns></returns>
int check_args() {
	//Check freq
	if (frequency < 76000000 || frequency > 108000000) {
		printf("Frequency (%i) isn't set correctly. Specify -f with 78.0-108.0.\n", frequency);
		return -1;
	}

	//Check if the MPX icecast is set and OK
	if (icecast_mpx != 0 && !icecast_mpx->is_configured()) {
		printf("MPX Icecast isn't fully configured. Set all options or remove it.\n");
		return -1;
	}

	//Check if the audio icecast is set and OK
	if (icecast_aud != 0 && !icecast_aud->is_configured()) {
		printf("Audio Icecast isn't fully configured. Set all options or remove it.\n");
		return -1;
	}

	//Check that either the MPX or audio icecast is set
	if (icecast_mpx == 0 && icecast_aud == 0) {
		printf("Neither audio or MPX Icecast output is set.\n");
		return -1;
	}

	//Check that deviation is valid
	if (radio_settings.fm_deviation == 0) {
		printf("FM deviation is invalid.\n");
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[]) {
	//Configure settings with reasonable defaults
	radio_settings.deemphasis_rate = DEFAULT_DEEMPHASIS_RATE;
	radio_settings.fm_deviation = DEFAULT_FM_DEVIATION;
	radio_settings.bb_filter_cutoff = DEFAULT_BB_FILTER_CUTOFF;
	radio_settings.bb_filter_trans = DEFAULT_BB_FILTER_TRANS;
	radio_settings.mpx_filter_cutoff = DEFAULT_MPX_FILTER_CUTOFF;
	radio_settings.mpx_filter_trans = DEFAULT_MPX_FILTER_TRANS;
	radio_settings.aud_filter_cutoff = DEFAULT_AUD_FILTER_CUTOFF;
	radio_settings.aud_filter_trans = DEFAULT_AUD_FILTER_TRANS;

	//Parse command line args
	if (parse_args(argc, argv))
		return -1;
	
	//Check command line args
	if (check_args())
		return -1;

	//Open radio
	printf("Opening AirSpy HF+ Device (on %i kHz)...\n", frequency / 1000);
	fmice_radio* radio = new fmice_radio(radio_settings);
	radio->open(frequency);

	//Initialize icecast and attach
	printf("Connecting to Icecast...\n");
	if (icecast_aud != 0) {
		try {
			icecast_aud->init();
		}
		catch (std::runtime_error* ex) {
			printf("Error: Failed to initialize audio icecast: %s\n", ex->what());
			return -1;
		}
		radio->set_audio_output(icecast_aud);
	}
	if (icecast_mpx != 0) {
		try {
			icecast_mpx->init();
		}
		catch (std::runtime_error* ex) {
			printf("Error: Failed to initialize composite icecast: %s\n", ex->what());
			return -1;
		}
		radio->set_mpx_output(icecast_mpx);
	}

	//Start the radio
	printf("Starting radio...\n");
	radio->start();

	//Loop
	while (1)
		radio->work();

	//Done
	printf("Exiting...\n");

	return 0;
}