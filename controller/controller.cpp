#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <RF24/RF24.h>
#include <curses.h>
#include <menu.h>

#include "../bottles/bottles_core.h"
#include "../glowworm/glowworm_core.h"
#include "../cauldron/cauldron_core.h"
#include "controller.h"

using namespace std;

#define INTERVAL 3	/* The send interval (in ms) */

RF24 * const radio = new RF24(RPI_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

bottles_config * const bottles = new bottles_config();
glowworm_config * const glowworm = new glowworm_config();
cauldron_config * const cauldron = new cauldron_config();

WINDOW * menuWindow;
WINDOW * infoWindow;

int main(int argc, char** argv){
	// Setup the radio
	setupRadio();

	// Initialise the ncurses window
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	menuWindow = newwin(20, 40, 0, 0);
	infoWindow = newwin(20, 40, 0, 40);

	while (createMenu(PROGRAM_EXIT)) {
	}

	endwin();

	return 0;
}

bool createMenu(program_e prog) {
	// The ncurses menu
	ITEM ** myItems;
	int c;
	MENU * myMenu;
	uint8_t menuCount;
	const char ** descriptions;
	ITEM * currentItem;

	// Which menu should be displayed
	switch (prog) {
		case PROGRAM_BOTTLES:
			descriptions = BOTTLES_MENU;
			menuCount = ARRAY_SIZE(BOTTLES_MENU);
			break;

		case PROGRAM_CAULDRON:
			descriptions = CAULDRON_MENU;
			menuCount = ARRAY_SIZE(CAULDRON_MENU);
			break;

		case PROGRAM_GLOWWORM:
			descriptions = GLOWWORM_MENU;
			menuCount = ARRAY_SIZE(GLOWWORM_MENU);
			break;

		default:
			descriptions = MAIN_MENU;
			menuCount = ARRAY_SIZE(MAIN_MENU);
			break;

	}

	// Allocate the menu items and create them
	myItems = (ITEM **)calloc(menuCount + 1, sizeof(ITEM *));
	for (uint8_t i = 0; i < menuCount; i++) {
		myItems[i] = new_item(descriptions[i], NULL); // Don't need a description
	}
	myItems[menuCount] = (ITEM *)NULL;

	myMenu = new_menu((ITEM **)myItems);

	// Prepare the menu window
	wclear(menuWindow);
	box(menuWindow, 0, 0);
 	set_menu_win(myMenu, menuWindow);
	set_menu_sub(myMenu, derwin(menuWindow, 16, 38, 3, 1));

	// Menu window title uses the current selection
	// and a separator
	mvwaddch(menuWindow, 2, 0, ACS_LTEE);
	mvwhline(menuWindow, 2, 1, ACS_HLINE, 38);
	mvwaddch(menuWindow, 2, 39, ACS_RTEE);
	mvwprintw(menuWindow, 1, 3,
		prog == PROGRAM_EXIT ? "Select a Program" : MAIN_MENU[prog]);

	// And draw it
	refresh();
	post_menu(myMenu);
	wrefresh(menuWindow);

	// Redraw the information window
	wclear(infoWindow);
	box(infoWindow, 0,0);
	wrefresh(infoWindow);

	// Focus on the menu window
	keypad(menuWindow, TRUE);

	program_e newProg = PROGRAM_EXIT;
	while ((c = getch())) {
		bool quit = false;
		switch (c) {
			case KEY_DOWN:
				menu_driver(myMenu, REQ_DOWN_ITEM);
				break;

			case KEY_UP:
				menu_driver(myMenu, REQ_UP_ITEM);
				break;

			case 10: // Enter
				currentItem = current_item(myMenu);
				int index = item_index(currentItem);
				if (prog == PROGRAM_EXIT) {
					quit = true;
					newProg = (program_e)index;
				} else if (index == 0)  {
					quit = true;
				} else {
					// Just do what ever was requested
					programMenuSelected(prog, currentItem);
				}
				break;
		}

		if (quit) {
			break;
		} else {
			// Need to refresh the window to register the selection change
			wrefresh(menuWindow);
		}
	}

	unpost_menu(myMenu);
	for (uint8_t i = 0; i < menuCount; i++) {
		free_item(myItems[i]);
	}
	free_menu(myMenu);

	if (prog == PROGRAM_EXIT) {
		if (newProg == PROGRAM_EXIT) {
			return false; // Signal that we want to quit
		} else {
			createMenu(newProg); // Draw the requested program menu
		}
	}

	return true;
}

bool doBottles(bottles_e command) {
	switch (command) {
		case BOTTLES_OFF:
			return writeBottles(BOTTLES_MODE_NONE);

		case BOTTLES_RIGGING:
			return writeBottles(bottles->mode == BOTTLES_MODE_RIGGING ? BOTTLES_MODE_NONE : BOTTLES_MODE_RIGGING);

		case BOTTLES_RED:
			return writeBottles(bottles->mode == BOTTLES_MODE_RED ? BOTTLES_MODE_NONE : BOTTLES_MODE_RED);

		case BOTTLES_WHITE:
			return writeBottles(bottles->mode == BOTTLES_MODE_WHITE ? BOTTLES_MODE_NONE : BOTTLES_MODE_WHITE);

		default:
			return true;
	}
}

bool doCauldron(cauldron_e command) {
	switch (command) {
		case CAULDRON_OFF:
			return writeCauldron(CAULDRON_MODE_NONE);

		case CAULDRON_TOGGLE_ALL:
			return writeCauldron((cauldron_mode_e)(cauldron->mode ^ CAULDRON_MODE_ALL));

		case CAULDRON_TOGGLE_FAN:
			return writeCauldron((cauldron_mode_e)(cauldron->mode ^ CAULDRON_MODE_FAN));

		case CAULDRON_TOGGLE_FIRE:
			return writeCauldron((cauldron_mode_e)(cauldron->mode ^ CAULDRON_MODE_FIRE));

		case CAULDRON_TOGGLE_GLOW:
			return writeCauldron((cauldron_mode_e)(cauldron->mode ^ CAULDRON_MODE_GLOW));

		default:
			return true;
	}
}

bool doGlowworm(glowworm_e command) {
	switch (command) {
		case GLOWWORM_OFF:
			return writeGlowworm(ANTENNA_NONE, 0, 0, 0);
		
		case GLOWWORM_TOGGLE_BOTH:
			return writeGlowworm((antenna_e)(glowworm->antenna ^ ANTENNA_BOTH));

		case GLOWWORM_TOGGLE_LEFT:
			return writeGlowworm((antenna_e)(glowworm->antenna ^ ANTENNA_LEFT));

		case GLOWWORM_TOGGLE_RIGHT:
			return writeGlowworm((antenna_e)(glowworm->antenna ^ ANTENNA_RIGHT));

		case GLOWWORM_BLINK:
			return writeGlowworm((antenna_e)(glowworm->antenna ^ ANTENNA_BLINK));

		case GLOWWORM_RED:
			return writeGlowworm(0xa0, 0, 0);

		case GLOWWORM_GREEN:
			return writeGlowworm(0, 0xa0, 0);

		case GLOWWORM_BLUE:
			return writeGlowworm(0,0, 0xa0);

		case GLOWWORM_YELLOW:
			return writeGlowworm(0xa0, 0xa0, 0);

		case GLOWWORM_PINK:
			return writeGlowworm(0xff, 0x0a,0x50); // Deep Pink

		default:
			return true;
	}
}

void programMenuSelected(program_e prog, ITEM * item) {
	// Set up the radio for the particular program
	setupRadio(prog);

	int index = item_index(item);

	switch (prog) {
		case PROGRAM_BOTTLES:
			doBottles((bottles_e)index);
			break;

		case PROGRAM_CAULDRON:
			doCauldron((cauldron_e)index);
			break;

		case PROGRAM_GLOWWORM:
			doGlowworm((glowworm_e)index);
			break;

		default:
			break;

	}
}

void setupRadio() {
	// HACK: Do the startup of the scanner app
	// I think this makes it go into standby
	radio->begin();
	radio->setAutoAck(false);
	radio->startListening();
	radio->stopListening();
	
	radio->begin();
	radio->setAutoAck(true);
	radio->setChannel(CHANNEL);
	
	//radio->enableAckPayload(); // So we can send a response back
	radio->setRetries(RETRY_DELAY, RETRY_COUNT); // How often and how many times to retry
	
	// Use High power, and slowest rate
	radio->setPALevel(RF24_PA_HIGH);
	radio->setDataRate(RF24_250KBPS);
}

void setupRadio(program_e prog) {
	radio->stopListening();
	size_t payload;

	switch (prog) {
		case PROGRAM_BOTTLES:
			payload = sizeof(bottles_config);
			break;

		case PROGRAM_CAULDRON:
			payload = sizeof(cauldron_config);
			break;

		case PROGRAM_GLOWWORM:
			payload = sizeof(glowworm_config);
			break;

		default:
			return;

	}

	radio->setPayloadSize(payload);
	radio->openReadingPipe(1, addresses[0]);
	radio->openWritingPipe(addresses[prog]);

	radio->startListening();
}

bool writeBottles(bottles_mode_e mode) {
	bottles->mode = mode;

	wclear(infoWindow);
	box(infoWindow, 0,0);

	mvwprintw(infoWindow, 1, 1, "Rigging: %s", (mode == BOTTLES_MODE_RIGGING) ? "On" : "Off");
	mvwprintw(infoWindow, 2, 1, "Red:     %s", (mode == BOTTLES_MODE_RED) ? "On" : "Off");
	mvwprintw(infoWindow, 3, 1, "White:   %s", (mode == BOTTLES_MODE_WHITE) ? "On" : "Off");

	wrefresh(infoWindow);

	bottles_config * response = new bottles_config();

	bool success = writeRadio(bottles, sizeof(bottles), response);

	delete response;

	return success;
}

bool writeCauldron(cauldron_mode_e mode) {
	cauldron->mode = mode;

	wclear(infoWindow);
	box(infoWindow, 0,0);

	mvwprintw(infoWindow, 1, 1, "Fan:   %s", (mode & CAULDRON_MODE_FAN) ? "On" : "Off");
	mvwprintw(infoWindow, 2, 1, "Fire:  %s", (mode & CAULDRON_MODE_FIRE) ? "On" : "Off");
	mvwprintw(infoWindow, 3, 1, "Glow:  %s", (mode & CAULDRON_MODE_GLOW) ? "On" : "Off");

	wrefresh(infoWindow);

	cauldron_config * response = new cauldron_config();

	bool success = writeRadio(cauldron, sizeof(cauldron), response);

	delete response;

	return success;
}

bool writeGlowworm(antenna_e antenna) {
	return writeGlowworm(antenna, glowworm->r, glowworm->g, glowworm->b);
}

bool writeGlowworm(uint8_t r, uint8_t g, uint8_t b) {
	return writeGlowworm((antenna_e)glowworm->antenna, r, g, b);
}

bool writeGlowworm(antenna_e antenna, uint8_t r, uint8_t g, uint8_t b) {
	glowworm->antenna = antenna;
	glowworm->r = r;
	glowworm->g = g;
	glowworm->b = b;

	wclear(infoWindow);
	box(infoWindow, 0,0);

	mvwprintw(infoWindow, 1, 1, "Antenna: %i", antenna);
	mvwprintw(infoWindow, 2, 1, "Red:     0x%02x", r);
	mvwprintw(infoWindow, 3, 1, "Green:   0x%02x", g);
	mvwprintw(infoWindow, 4, 1, "Blue:    0x%02x", b);

	mvprintw(LINES - 3, 0,
		"Sending: Antenna: %i, R: 0x%02x, G: 0x%02x, B: 0x%02x ...\n",
		antenna, r, g, b);

	wrefresh(infoWindow);

	glowworm_config * response = new glowworm_config();

	bool success = writeRadio(glowworm, sizeof(glowworm), response);

	delete response;

	return success;
}

bool writeRadio(void * payload, size_t length, void * response) {
	// Stop listening so we can send
	radio->stopListening();

	// Current time so we can measure the response time
	uint32_t time = millis();

	mvprintw(LINES - 2, 0, "Sending %i bytes...", length);
	bool ok = radio->write(payload, length);
	mvprintw(LINES - 2, 20, "%s\n", ok ? "done" : "failed");

	// Start listening agains so we can receive the response
	radio->startListening();

	uint32_t startedWaiting = millis();
	bool timeout = false;
	while (!radio->available() &&  !timeout) {
		if ((millis() - startedWaiting) > TIMEOUT) {
			timeout = true;
		}
	}

	uint32_t pingTime = millis() - time;

	// What happened
	if (timeout) {
		mvprintw(LINES - 1, 0, "Failed - response timed out after %u.\n", TIMEOUT);
	} else {
		radio->read(response, length);
		mvprintw(LINES - 1, 0, "Received %i bytes - round-trip delay: %u\n", length, pingTime);
		mvprintw(LINES - 1, 0, "Success");
	}

	return true;
}
