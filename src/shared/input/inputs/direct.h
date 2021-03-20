#pragma once
#include "controller/controller.h"
#include "eeprom/eeprom.h"
#include "guitar.h"
#include "output/descriptors.h"
#include "pins/pins.h"
#include "util/util.h"
#include <stdlib.h>
Pin_t pinData[16] = {};
int validPins = 0;
uint8_t detectedPin = 0xff;
bool lookingForDigital = false;
bool lookingForAnalog = false;
int lastAnalogValue[NUM_ANALOG_INPUTS];
bool lastDigitalValue[NUM_DIGITAL_PINS];
AnalogInfo_t joyData[NUM_ANALOG_INPUTS];
int16_t analogueData[XBOX_AXIS_COUNT];
void initDirectInput(void) {
  uint8_t *pins = (uint8_t *)&config.pins;
  validPins = 0;
  setUpValidPins();
  if (config.main.inputType == DIRECT) {
    for (size_t i = 0; i < XBOX_BTN_COUNT; i++) {
      if (pins[i] != INVALID_PIN) {
        bool is_fret = (i >= XBOX_A || i == XBOX_LB || i == XBOX_RB);
        Pin_t pin = setUpDigital(
            pins[i], i, is_fret && config.main.fretLEDMode == LEDS_INLINE);
        if (isDrum(config.main.subType) && is_fret) {
          // We should probably keep a list of drum specific buttons, instead of
          // using isfret
          // ADC is 10 bit, thereshold is specified as an 8 bit value, so shift
          // it
          setUpAnalogDigitalPin(pin, pins[i], config.axis.drumThreshold << 3);
        } else {
          pinMode(pins[i], pin.eq ? INPUT : INPUT_PULLUP);
          pinData[validPins++] = pin;
          if (isGuitar(config.main.subType) &&
              (i == XBOX_DPAD_DOWN || i == XBOX_DPAD_UP)) {
            pin.milliDeBounce = 20;
          }
        }
      }
    }
  }
}
bool shouldSkipPin(uint8_t i) {
  // On the 328p, due to an inline LED, it isn't possible to check pin 13
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__) ||              \
    defined(__AVR_ATmega328P__)
  if (i == 13 || i == 0 || i == 1) return true;
#endif
  // Skip sda + scl when using peripherials utilising I2C
  if ((config.main.tiltType == MPU_6050 || config.main.inputType == WII) &&
      (i == PIN_WIRE_SDA || i == PIN_WIRE_SCL)) {
    return true;
  }
  // Skip RF related pins (such as spi) when using an RF transmitter
#ifdef RF_TX
#  if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__) ||            \
      defined(__AVR_ATmega328P__)
  if (i == PIN_SPI_MOSI || i == PIN_SPI_MISO || i == PIN_SPI_SCK ||
      i == PIN_SPI_SS || i == 8 || i == 2)
    return true;
#  else
  if (i == PIN_SPI_MOSI || i == PIN_SPI_MISO || i == PIN_SPI_SCK ||
      i == PIN_SPI_SS || i == 0 || i == 1)
    return true;
#  endif
#endif
  // Skip SPI pins when using peripherials that utilise SPI
  if ((config.main.fretLEDMode == APA102) &&
      (i == PIN_SPI_MOSI || i == PIN_SPI_MISO || i == PIN_SPI_SCK ||
       i == PIN_SPI_SS)) {
    return true;
  }
  return false;
}

void findDigitalPin(void) {
  if (lookingForDigital) return;
  detectedPin = 0xff;
  stopReading();
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    if (!shouldSkipPin(i)) {
      pinMode(i, INPUT_PULLUP);
      lastDigitalValue[i] = digitalRead(i);
    }
  }
  lookingForDigital = true;
}

void findAnalogPin(void) {
  if (lookingForAnalog) return;
  detectedPin = 0xff;
  stopReading();
  for (int i = 0; i < NUM_ANALOG_INPUTS; i++) {
    pinMode(PIN_A0 + i, INPUT_PULLUP);
    lastAnalogValue[i] = analogRead(i);
  }
  lookingForAnalog = true;
}

void stopSearching(void) {
  if (lookingForDigital) {
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
      if (!shouldSkipPin(i)) { pinMode(i, INPUT); }
      lastDigitalValue[i] = digitalRead(i);
    }
  }
  lookingForDigital = lookingForAnalog = false;
  initDirectInput();
}

void setSP(bool sp) {
  if (config.pinsSP != INVALID_PIN) { digitalWrite(config.pinsSP, sp); }
}

void tickDirectInput(Controller_t *controller) {
  if (lookingForAnalog) {
    for (int i = 0; i < NUM_ANALOG_INPUTS; i++) {
      if (abs(analogRead(i) - lastAnalogValue[i]) > 10) {
        initDirectInput();
        detectedPin = i + PIN_A0;
        lookingForAnalog = false;
        return;
      }
    }
    return;
  }
  if (lookingForDigital) {
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
      if (!shouldSkipPin(i)) {
        if (digitalRead(i) != lastDigitalValue[i]) {
          stopSearching();
          detectedPin = i;
          lookingForDigital = false;
          return;
        }
      }
    }
    return;
  }
  tickAnalog();
  Pin_t pin;
  for (uint8_t i = 0; i < validPins; i++) {
    pin = pinData[i];
    bool val = digitalReadPin(pin);
    if (millis() - pin.lastMillis > pin.milliDeBounce) {
      if (val != (controller->buttons & pin.pmask)) {
        pin.lastMillis = millis();
      }
      controller->buttons ^= (-val ^ controller->buttons) & pin.pmask;
    }
  }
  AnalogInfo_t info;
  ControllerCombined_t *combinedController = (ControllerCombined_t *)controller;
  AxisScale_t *scales = &config.axisScale.lt;
  for (int8_t i = 0; i < validAnalog; i++) {
    info = joyData[i];
    analogueData[info.offset] = info.value;
    int32_t val = info.value;
    val -= scales[info.offset].offset;
    val *= scales[info.offset].multiplier;
    val /= 1024;
    val += INT16_MIN;
    if (val > INT16_MAX) val = INT16_MAX;
    if (val < INT16_MIN) val = INT16_MIN;
    if (info.hasDigital) {
      if (info.value > info.threshold) {
        controller->buttons |= info.digital.pmask;
      }
      controller->drumVelocity[info.offset - 8] = val;
    } else if (info.offset >= 2) {
      combinedController->sticks[info.offset - 2] = val;
    } else {
      combinedController->triggers[info.offset] = ((uint16_t)val) >> 8;
    }
  }
}
