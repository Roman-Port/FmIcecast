#include "stdio.h"
#include "defines.h"
#include "cast.h"
#include "radio.h"

int main() {
	//Open radio
	printf("Opening AirSpy HF+ Device...\n");
	fmice_radio* radio = new fmice_radio();
	radio->open(103300000);

	//Connect to Icecast
	printf("Connecting to Icecast...\n");
	fmice_icecast* ice = new fmice_icecast("ice.romanport.com", 80, "/kzcr-composite", "source", "Urgkq5HA2u3Q4oksjjpCrXqt", 2, AUDIO_SAMP_RATE);
	radio->set_audio_output(ice);

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