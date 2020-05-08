#include "leds.h"
#include "../../../lib/AVR-APA102-library/src/apa102.h"
#include "../config/eeprom.h"
#include "../util.h"
// #include "input_handler.h"
#include <avr/power.h>
void led_init(void) {
  if (config.main.fret_mode != APA102) return;
  apa102_set_led_count(NUM_LEDS);
  clock_prescale_set(clock_div_1);
  apa102_init_spi();
}
long unsigned int colours[] = {Black, Green, White, DeepSkyBlue, 0xFF00FF};
long unsigned int fretColours[] = {Green, Red, Yellow, Blue, OrangeRed};
int frets[] = {XBOX_A, XBOX_B, XBOX_Y, XBOX_X, XBOX_LB};
void led_tick(controller_t *controller) {
  if (config.main.fret_mode != APA102) return;
  apa102_start();
  // for (int i = NUM_LEDS-1; i>=0; i--) {
  for (int i = 0; i<NUM_LEDS; i++) {
    uint32_t col = controller->leds.gui;
    if (col == Black) {
      col = colours[controller->leds.leds[i]];
      if ((col == Black && bit_check(controller->buttons, frets[i])) ||
          col == Green) {
        col = fretColours[i];
      }
    }
    apa102_set_led(rgb(col));
  }
  apa102_end();
}