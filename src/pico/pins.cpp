#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <stdint.h>

#include "Arduino.h"
#include "config.h"
#include "io_define.h"
#include "pin_funcs.h"
#include "util.h"
uint16_t adcReading[NUM_ANALOG_INPUTS];
bool first = true;
uint16_t adc(uint8_t pin) {
    adc_select_input(pin);
    return adc_read() << 4;
}

void initPins(void) {
    adc_init();
    PIN_INIT;
}

uint8_t digital_read(uint8_t port, uint8_t mask) {
    port = port * 8;
    uint32_t mask32 = mask << port;
    for (uint i = 0; i < NUM_BANK0_GPIOS; i++) {
        if (mask32 & (1 << i)) {
            gpio_init(i);
            gpio_set_pulls(i, true, false);
        }
    }
    uint32_t ret = sio_hw->gpio_in;
    PIN_INIT;
    return ((ret >> port) & 0xff);
}

void digital_write(uint8_t port, uint8_t mask, uint8_t activeMask) {
    port = port * 8;
    uint32_t mask32 = mask << port;
    uint32_t activeMask32 = activeMask << port;
    for (uint i = 0; i < NUM_BANK0_GPIOS; i++) {
        if (mask32 & (1 << i)) {
            gpio_init(i);
            gpio_set_dir(i, true);
        }
    }
    gpio_put_masked(mask32, activeMask32);
}

uint16_t adc_read(uint8_t pin, uint8_t mask) {
    bool detecting = pin & (1 << 7);
    if (detecting) {
        pin = pin & ~(1 << 7);
        gpio_init(pin + PIN_A0);
        gpio_set_pulls(pin + PIN_A0, true, false);
        gpio_set_input_enabled(pin + PIN_A0, false);
    }
    adc_select_input(pin);
    uint16_t data = adc_read() << 4;
    if (detecting) {
        PIN_INIT;
    }
    return data;
}

uint16_t multiplexer_read(uint8_t pin, uint32_t mask, uint32_t bits) {
    if (!disable_multiplexer) {
        gpio_put_masked(mask, bits);
    }
    adc_select_input(pin);
    return adc_read() << 4;
}