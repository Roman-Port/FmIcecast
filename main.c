#include "stdio.h"
#include "defines.h"

int main() {
	//Initialize radio
	if (radio_init())
		return -1;

	//Connect to Icecast
	if (cast_init("ice.romanport.com", 80, "/kzcr-composite", "source", "Urgkq5HA2u3Q4oksjjpCrXqt"))
		return -1;

	//Start the radio
	printf("Starting radio...\n");
	radio_start();

	//Loop
	while (1) {
		cast_transmit();
	}

	return 0;
}