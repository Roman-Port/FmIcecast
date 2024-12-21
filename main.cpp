#include "stdio.h"
#include "defines.h"
#include "cast.h"
#include "radio.h"
#include "codecs/codec_flac.h"
#include "devices/device_airspyhf.h"

#include <getopt.h>

#define DEFAULT_FM_DEVIATION 85000 // Gives headroom for overmodulation
#define DEFAULT_DEEMPHASIS_RATE 75 // For USA
#define DEFAULT_BB_FILTER_CUTOFF 125000
#define DEFAULT_BB_FILTER_TRANS 15000
#define DEFAULT_MPX_FILTER_CUTOFF 61500
#define DEFAULT_MPX_FILTER_TRANS 2000
#define DEFAULT_AUD_FILTER_CUTOFF 15000
#define DEFAULT_AUD_FILTER_TRANS 4000
#define DEFAULT_RDS_BUFFER 1
#define DEFAULT_RDS_LEVEL -10
#define DEFAULT_STEREO_PILOT_LEVEL -30

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
	printf("        [-s Enable status output every 1s]\n");
	printf("    Add Icecast Output:\n");
	printf("        [--ice-mpx Composite Icecast codec <flac>]\n");
	printf("        [--ice-aud Audio Icecast codec <flac>]\n");
	printf("    Configure Icecast Output Settings:\n");
	printf("        [-h Composite Icecast hostname]\n");
	printf("        [-o Composite Icecast port]\n");
	printf("        [-m Composite Icecast mountpoint]\n");
	printf("        [-u Composite Icecast username]\n");
	printf("        [-p Composite Icecast password]\n");
	printf("    RDS Re-Encoder:\n");
	printf("        [--rds]\n");
	printf("        [--rds-level RDS Level in dB (default is %i dB)]\n", DEFAULT_RDS_LEVEL);
	printf("        [--rds-buffer RDS maximum skew in seconds (default is %is)]\n", DEFAULT_RDS_BUFFER);
	printf("    Other Features:\n");
	printf("        [--stereo-gen The stereo pilot level (default is %i dB)]\n", DEFAULT_STEREO_PILOT_LEVEL);
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

static fmice_icecast* create_icecast(const char* codecName, int channels, int sampRate) {
	//Determine the codec to create
	fmice_codec* codec;
	if (strcmp(codecName, "flac") == 0)
		codec = new fmice_codec_flac(sampRate, channels);
	else {
		printf("Unknown codec \"%s\". Options are: flac.\n", codecName);
		return 0;
	}

	return new fmice_icecast(2, AUDIO_SAMP_RATE, codec);
}

int parse_args(int argc, char* argv[]) {
	static const struct option long_opts[] = {
		{ "ice-mpx", required_argument, NULL, 11 },
		{ "ice-aud", required_argument, NULL, 12 },
		{ "deviation", required_argument, NULL, 31 },
		{ "deemphasis", required_argument, NULL, 32 },
		{ "bb-filter-cutoff", required_argument, NULL, 33 },
		{ "bb-filter-trans", required_argument, NULL, 34 },
		{ "mpx-filter-cutoff", required_argument, NULL, 35 },
		{ "mpx-filter-trans", required_argument, NULL, 36 },
		{ "aud-filter-cutoff", required_argument, NULL, 37 },
		{ "aud-filter-trans", required_argument, NULL, 38 },
		{ "freq", required_argument, NULL, 'f'},
		{ "rds", no_argument, NULL, 15 },
		{ "rds-level", required_argument, NULL, 16 },
		{ "rds-buffer", required_argument, NULL, 17 },
		{ "stereo-gen", required_argument, NULL, 18 },
		{ 0 }
	};

	int opt;
	fmice_icecast* currentOutput = 0;
	while ((opt = getopt_long(argc, argv, "f:h:o:m:u:p:s", long_opts, NULL)) != -1) {
		switch (opt) {
		
		case 'f':
			// FREQUENCY
			frequency = parse_freq(optarg);
			break;

		case 's':
			// ENABLE STATUS
			radio_settings.enable_status = true;
			break;

		case 11:
			// MPX ICECAST
			if (icecast_mpx == 0)
				icecast_mpx = create_icecast(optarg, 1, MPX_SAMP_RATE);
			if (icecast_mpx == 0)
				return -1;
			currentOutput = icecast_mpx;
			break;

		case 12:
			// AUDIO ICECAST
			if (icecast_aud == 0)
				icecast_aud = create_icecast(optarg, 2, AUDIO_SAMP_RATE);
			if (icecast_aud == 0)
				return -1;
			currentOutput = icecast_aud;
			break;
		
		// BELOW ARE SETTINGS FOR ICECAST - Intended to be grouped together
		case 'h':
			if (currentOutput != 0) {
				currentOutput->set_host(optarg);
				break;
			}
		case 'o':
			if (currentOutput != 0) {
				currentOutput->set_port(atoi(optarg));
				break;
			}
		case 'm':
			if (currentOutput != 0) {
				currentOutput->set_mount(optarg);
				break;
			}
		case 'u':
			if (currentOutput != 0) {
				currentOutput->set_username(optarg);
				break;
			}
		case 'p':
			if (currentOutput != 0) {
				currentOutput->set_password(optarg);
				break;
			}
			else {
				printf("Icecast config settings must be used after specifying the output.\n");
				return -1;
			}

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

		case 15:
			// RDS ENABLE
			radio_settings.rds_enable = true;
			break;

		case 16:
			// RDS LEVEL
			radio_settings.rds_level = atoi(optarg);
			break;

		case 17:
			// RDS BUFFER
			radio_settings.rds_max_skew = atoi(optarg);
			break;

		case 18:
			// STEREO GENERATOR
			radio_settings.stereo_generator_enable = true;
			radio_settings.stereo_generator_level = atoi(optarg);
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
	radio_settings.enable_status = false;
	radio_settings.deemphasis_rate = DEFAULT_DEEMPHASIS_RATE;
	radio_settings.fm_deviation = DEFAULT_FM_DEVIATION;
	radio_settings.bb_filter_cutoff = DEFAULT_BB_FILTER_CUTOFF;
	radio_settings.bb_filter_trans = DEFAULT_BB_FILTER_TRANS;
	radio_settings.mpx_filter_cutoff = DEFAULT_MPX_FILTER_CUTOFF;
	radio_settings.mpx_filter_trans = DEFAULT_MPX_FILTER_TRANS;
	radio_settings.aud_filter_cutoff = DEFAULT_AUD_FILTER_CUTOFF;
	radio_settings.aud_filter_trans = DEFAULT_AUD_FILTER_TRANS;
	radio_settings.rds_enable = false;
	radio_settings.rds_level = DEFAULT_RDS_LEVEL;
	radio_settings.rds_max_skew = DEFAULT_RDS_BUFFER;
	radio_settings.stereo_generator_enable = false;
	radio_settings.stereo_generator_level = DEFAULT_STEREO_PILOT_LEVEL;

	//Parse command line args
	if (parse_args(argc, argv))
		return -1;
	
	//Check command line args
	if (check_args())
		return -1;

	//Open radio
	printf("Opening AirSpy HF+ Device (on %i kHz)...\n", frequency / 1000);
	fmice_device_airspyhf airspy(SAMP_RATE);
	airspy.open(frequency);

	//Set up radio
	fmice_radio radio(&airspy, radio_settings);

	//Initialize icecast and attach
	if (icecast_aud != 0) {
		try {
			icecast_aud->init();
		}
		catch (std::runtime_error* ex) {
			printf("Error: Failed to initialize audio icecast: %s\n", ex->what());
			return -1;
		}
		radio.set_audio_output(icecast_aud);
	}
	if (icecast_mpx != 0) {
		try {
			icecast_mpx->init();
		}
		catch (std::runtime_error* ex) {
			printf("Error: Failed to initialize composite icecast: %s\n", ex->what());
			return -1;
		}
		radio.set_mpx_output(icecast_mpx);
	}

	//Start the radio
	printf("Starting radio...\n");
	airspy.start();

	//Loop
	printf("Running...\n");
	while (1)
		radio.work();

	//Done
	printf("Exiting...\n");

	return 0;
}