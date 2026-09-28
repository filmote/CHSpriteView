#include "CHGame.h"

CHGame::CHGame() : CHGfx() {}


uint8_t currentButtonState = 0;
uint8_t previousButtonState = 0;

uint8_t CHGame::buttonsState() {

	uint8_t buttons = 0;

	if (digitalRead(PIN_BTN_A) == LOW) {
		buttons |= A_BUTTON;
	}
	if (digitalRead(PIN_BTN_B) == LOW) {
		buttons |= B_BUTTON;
	}
	if (digitalRead(PIN_BTN_LEFT) == LOW) {
		buttons |= LEFT_BUTTON;
	}
	if (digitalRead(PIN_BTN_RIGHT) == LOW) {
		buttons |= RIGHT_BUTTON;
	}
	if (digitalRead(PIN_BTN_UP) == LOW) {
		buttons |= UP_BUTTON;
	}
	if (digitalRead(PIN_BTN_DOWN) == LOW) {
		buttons |= DOWN_BUTTON;
	}
	if (digitalRead(PIN_BTN_SELECT) == LOW) {
		buttons |= SELECT_BUTTON;
	}
	if (digitalRead(PIN_BTN_START) == LOW) {
		buttons |= START_BUTTON;
	}
	return buttons;
}

void CHGame::boot() {

	pinMode(PIN_BTN_A, INPUT_PULLUP);
	pinMode(PIN_BTN_B, INPUT_PULLUP);
	pinMode(PIN_BTN_UP, INPUT_PULLUP);
	pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
	pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
	pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
	pinMode(PIN_BTN_START, INPUT_PULLUP);
	pinMode(PIN_BTN_SELECT, INPUT_PULLUP);

	currentButtonState = buttonsState();
	previousButtonState = currentButtonState;

}

bool CHGame::anyPressed(uint8_t buttons) {
	return (buttonsState() & buttons) != 0;
}

bool CHGame::pressed(uint8_t buttons) {
	return (buttonsState() & buttons) == buttons;
}

bool CHGame::notPressed(uint8_t buttons) {
	return (buttonsState() & buttons) == 0;
}

bool CHGame::justPressed(uint8_t buttons) {
  	return (!(previousButtonState & buttons) && (currentButtonState & buttons));
}

bool CHGame::justReleased(uint8_t buttons) {
	return (((previousButtonState & buttons) != 0) && ((currentButtonState & buttons) == 0));
}

void CHGame::pollButtons() {

	previousButtonState = currentButtonState;
	currentButtonState = buttonsState();

}

void CHGame::setFrameRate(uint8_t frameRate) {

	this->targetFPS = frameRate;
	this->frameDelay = 1000 / this->targetFPS;
	this->lastFrameTime = millis();

}

bool CHGame::nextFrame() {

	uint32_t currentMillis = millis();

	if (currentMillis - this->lastFrameTime >= this->frameDelay) {
	Serial.print(currentMillis);
	Serial.print(" - ");
	Serial.print(this->lastFrameTime);
	Serial.print(" = ");
	Serial.print(currentMillis - this->lastFrameTime);
	Serial.print(" > ");
	Serial.print(this->frameDelay);
	Serial.print("\n ");
	

		this->lastFrameTime = currentMillis;
		this->frameCount++;
		return true;
	} 
	else {
		return false;
	}

}
// void CHGame::setFrameRate(uint8_t frameRate) {
//     this->targetFPS = frameRate;
//     // Use microseconds (1,000,000 per second) for better 60 FPS accuracy
//     this->frameDelay = 1000000 / this->targetFPS; // 60 FPS = 16666 micros
//     this->lastFrameTime = micros();
// }

// bool CHGame::nextFrame() {
//     uint32_t currentMicros = micros();

//     if (currentMicros - this->lastFrameTime >= this->frameDelay) {
// 	Serial.print(currentMicros);
// 	Serial.print(" - ");
// 	Serial.print(this->lastFrameTime);
// 	Serial.print(" = ");
// 	Serial.print(currentMicros - this->lastFrameTime);
// 	Serial.print(" > ");
// 	Serial.print(this->frameDelay);
// 	Serial.print("\n ");	
//         // Fixes cumulative drift by maintaining a strict mathematical grid
//         this->lastFrameTime += this->frameDelay; 
//         this->frameCount++;
//         return true;
//     } 
//     return false;
// }
// bool CHGame::nextFrame() {

//     uint32_t currentMicros = micros();
//     uint32_t diff = currentMicros - this->lastFrameTime;

//     if (diff >= this->frameDelay) {
//         // Advanced Fix: Align to the most recent frame boundary.
//         // This stops it from breaking if a loop takes too long.
//         uint32_t missedPeriods = (currentMicros - this->lastFrameTime) / this->frameDelay;
//         this->lastFrameTime += missedPeriods * this->frameDelay;
        
//         this->frameCount++;
// 	Serial.print(currentMicros);
// 	Serial.print(" - ");
// 	Serial.print(this->lastFrameTime);
// 	Serial.print(" = ");
// 	Serial.print(diff);
// 	Serial.print(" > ");
// 	Serial.print(this->frameDelay);
// 	Serial.print("\n ");			
//         return true;
//     } 
//     return false;
// }


void CHGame::resetFrameCount() { this->frameCount = 0; }

uint16_t CHGame::getFrameCount() const { 
	return this->frameCount; 
}

uint16_t CHGame::getFrameCount(uint16_t mod) const {
	return this->frameCount % mod;
}

bool CHGame::getFrameCountHalf(uint8_t mod) const {
	return getFrameCount(mod) > (mod / 2);
}

bool CHGame::isFrameCount(uint16_t mod) const {
	return (this->frameCount % mod) == 0;
}

bool CHGame::isFrameCount(uint16_t mod, uint16_t val) const {
	return (this->frameCount % mod) == val;
}

/* ----------------------------------------------------------------------------
 *  Draw a horizontal dotted line.
 */
void CHGame::drawHorizontalDottedLine(uint8_t x1, uint8_t x2, uint8_t y,
                                      uint8_t colour) {
	uint8_t diff = (x2 - x1);

	for (uint8_t x = 0; x <= diff; x += 2) {
		drawPixel(x1 + x, y, colour);
	}

}

/* ----------------------------------------------------------------------------
 *  Draw a vertical dotted line.
 */
void CHGame::drawVerticalDottedLine(uint8_t y1, uint8_t y2, uint8_t x,
                                    uint8_t colour) {
	uint8_t diff = (y2 - y1);

	for (uint8_t y = 0; y <= diff; y += 2) {
		drawPixel(x, y1 + y, colour);
	}

}
