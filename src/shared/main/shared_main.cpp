#include "shared_main.h"

#include "Arduino.h"
#include "bt.h"
#include "config.h"
#include "controllers.h"
#include "endpoints.h"
#include "fxpt_math.h"
#include "hid.h"
#include "inputs/slave.h"
#include "io.h"
#include "io_define.h"
#include "pico_slave.h"
#include "pin_funcs.h"
#include "ps2.h"
#include "usbhid.h"
#include "util.h"
#include "wii.h"
#define DJLEFT_ADDR 0x0E
#define DJRIGHT_ADDR 0x0D
#define DJ_BUTTONS_PTR 0x12
#define GH5NECK_ADDR 0x0D
#define CLONE_ADDR 0x10
#define CLONE_VALID_PACKET 0x52
#define GH5NECK_BUTTONS_PTR 0x12
#define BUFFER_SIZE_QUEUE 255
#define KEY_ERR_OVF 0x01

struct {
    union {
        signed int ltt : 6;
        struct {
            unsigned int ltt40 : 5;
            unsigned int ltt5 : 1;
        };
    };
} ltt_t;
struct {
    union {
        signed int rtt : 6;
        struct {
            signed int rtt0 : 1;
            signed int rtt21 : 2;
            signed int rtt43 : 2;
            signed int rtt5 : 1;
        };
    };
} rtt_t;
uint8_t tmp = 0;
long clone_guitar_timer = 0;
long clone_guitar_ready_timer = 0;
bool clone_ready = false;
bool reading = false;
Buffer_Report_t last_queue_report;
long last_queue = 0;
uint8_t brightness = LED_BRIGHTNESS;
uint8_t queue_size = 0;
uint8_t queue_tail = 0;
Buffer_Report_t queue[BUFFER_SIZE_QUEUE];
#define TURNTABLE_BUFFER_SIZE 16
#ifdef INPUT_DJ_TURNTABLE_SMOOTHING
int16_t dj_sum_left = 0;
int8_t dj_last_readings_left[TURNTABLE_BUFFER_SIZE];
int8_t dj_next_left = 0;
int16_t dj_sum_right = 0;
int8_t dj_last_readings_right[TURNTABLE_BUFFER_SIZE];
int8_t dj_next_right = 0;
#endif
USB_Report_Data_t combined_report;
#if DEVICE_TYPE_IS_NORMAL_GAMEPAD
PS3_REPORT bt_report;
#else
USB_Report_Data_t bt_report;
#endif
uint8_t debounce[DIGITAL_COUNT];
uint8_t keyIndex[DIGITAL_COUNT];
uint8_t usedKeys[6];
uint16_t lastDrum[DIGITAL_COUNT];
uint8_t drumVelocity[8];
long lastDj = 0;
long lastSentPacket = 0;
long lastLed = 0;
long lastSentGHLPoke = 0;
long input_start = 0;
long lastDebounce = 0;
bool hasTapBar = false;
uint8_t ghl_sequence_number_host = 1;
uint16_t wiiControllerType = WII_NO_EXTENSION;
uint8_t ps2ControllerType = PSX_NO_DEVICE;
uint8_t lastSuccessfulPS2Packet[BUFFER_SIZE];
uint8_t lastSuccessfulWiiPacket[8];
uint8_t lastSuccessfulGH5Packet[2];
uint8_t lastSuccessfulClonePacket[4];
uint8_t lastSuccessfulTurntablePacketLeft[3];
uint8_t lastSuccessfulTurntablePacketRight[3];
uint8_t last_usb_report_size = 0;
long lastSuccessfulGHWTPacket;
bool lastGH5WasSuccessful = false;
bool lastCloneWasSuccessful = false;
bool lastTurntableWasSuccessfulLeft = false;
bool lastTurntableWasSuccessfulRight = false;
bool lastWiiWasSuccessful = false;
bool lastPS2WasSuccessful = false;
bool overrideR2 = false;
bool lastXboxOneGuide = false;
bool disable_multiplexer = false;
uint8_t overriddenR2 = 0;
USB_LastReport_Data_t last_report_usb;
USB_LastReport_Data_t last_report_bt;
USB_LastReport_Data_t temp_report_usb_host;
USB_LastReport_Data_t temp_report_usb_host_xb1;
#ifdef INPUT_USB_HOST
USB_Host_Data_t usb_host_data;
USB_Host_Data_t last_usb_host_data;
#endif
uint8_t rawWt;
uint8_t rawWtPeripheral;
bool auth_ps4_controller_found = false;
bool seen_ps4_console = false;
GipPowerMode_t powerMode;
long last_poll = 0;

/* Magic data taken from GHLtarUtility:
 * https://github.com/ghlre/GHLtarUtility/blob/master/PS3Guitar.cs
 * Note: The Wii U and PS3 dongles happen to share the same!
 */
uint8_t ghl_ps3wiiu_magic_data[] = {
    0x02, 0x08, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Magic data for the PS4 dongles sniffed with a USB protocol
 * analyzer.
 */
uint8_t ghl_ps4_magic_data[] = {
    0x30, 0x02, 0x08, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t clone_data[] = {0x53, 0x10, 0x00, 0x01};
#if LED_COUNT_STP
uint8_t ledState[ROUND_UP(LED_COUNT_STP, 8) / 8];
uint8_t ledStateSelect[ROUND_UP(LED_COUNT_STP, 8) / 8];
uint8_t lastLedState[ROUND_UP(LED_COUNT_STP, 8) / 8];
#else
Led_t ledState[LED_COUNT];
Led_t lastLedState[LED_COUNT];
#endif
#if LED_COUNT_PERIPHERAL_STP
uint8_t ledStatePeripheral[ROUND_UP(LED_COUNT_PERIPHERAL_STP, 8) / 8];
uint8_t ledStatePeripheralSelect[ROUND_UP(LED_COUNT_PERIPHERAL_STP, 8) / 8];
uint8_t lastLedStatePeripheral[ROUND_UP(LED_COUNT_PERIPHERAL_STP, 8) / 8];
#else
Led_t ledStatePeripheral[LED_COUNT_PERIPHERAL];
Led_t lastLedStatePeripheral[LED_COUNT_PERIPHERAL];
#endif
#define UP 1 << 0
#define DOWN 1 << 1
#define LEFT 1 << 2
#define RIGHT 1 << 3
static const uint8_t dpad_bindings[] = {0x08, 0x00, 0x04, 0x08, 0x06, 0x07, 0x05, 0x08, 0x02, 0x01, 0x03};
static const uint8_t dpad_bindings_reverse[] = {UP, UP | RIGHT, RIGHT, DOWN | RIGHT, DOWN, DOWN | LEFT, LEFT, UP | LEFT};

uint8_t gh5_mapping[] = {
    0x00, 0x95, 0xCD, 0xB0, 0x1A, 0x19, 0xE6,
    0xE5, 0x49, 0x47, 0x48, 0x46, 0x2F, 0x2D,
    0x2E, 0x2C, 0x7F, 0x7B, 0x7D, 0x79, 0x7E,
    0x7A, 0x7C, 0x78, 0x66, 0x62, 0x64, 0x60,
    0x65, 0x61, 0x63, 0x5F};
void setKey(uint8_t id, uint8_t key, USB_6KRO_Data_t *report, bool state) {
    if (state) {
        if (keyIndex[id]) {
            report->KeyCode[keyIndex[id]] = key;
            return;
        }
        for (size_t i = 0; i < sizeof(usedKeys); i++) {
            if (report->KeyCode[i] == key) {
                return;
            }
            if (!report->KeyCode[i]) {
                keyIndex[id] = i;
                report->KeyCode[i] = key;
                return;
            }
        }
        // Too many buttons - flag the overflow condition and clear all key indexes
        memset(report->KeyCode, KEY_ERR_OVF, sizeof(report->KeyCode));
        memset(keyIndex, 0, sizeof(keyIndex));
        return;
    }
    if (!keyIndex[id]) {
        return;
    }
    keyIndex[id] = 0;
}
void init_main(void) {
    #if !DEVICE_TYPE_IS_NORMAL_GAMEPAD
        consoleType = UNIVERSAL;
    #endif
    initPins();
    twi_init();
    spi_begin();
    GIP_HEADER((&powerMode), GIP_POWER_MODE_DEVICE_CONFIG, true, 1);
    powerMode.subcommand = 0x00;
    memset(ledState, 0, sizeof(ledState));
    memset(ledStatePeripheral, 0, sizeof(ledStatePeripheral));
    memset(usedKeys, 0, sizeof(usedKeys));
    memset(keyIndex, 0, sizeof(keyIndex));
    LED_INIT;
#ifdef INPUT_DJ_TURNTABLE_SMOOTHING
    memset(dj_last_readings_left, 0, sizeof(dj_last_readings_left));
    memset(dj_last_readings_right, 0, sizeof(dj_last_readings_right));
#endif
#ifdef INPUT_PS2
    init_ack();
#endif
}
#ifdef SLAVE_TWI_PORT
bool slave_initted = false;
void tick_slave() {
    if (slave_initted) {
        return;
    }
    if (!slaveInit()) {
        return;
    }
    slave_initted = true;
    PIN_INIT;
#ifdef SPI_SLAVE_0_MOSI
    slaveInitLED(0);
    slavePinMode(SPI_SLAVE_0_MOSI, PIN_MODE_SPI);
    slavePinMode(SPI_SLAVE_0_SCK, PIN_MODE_SPI);
#endif
#ifdef SPI_SLAVE_1_MOSI
    slaveInitLED(1);
    slavePinMode(SPI_SLAVE_1_MOSI, PIN_MODE_SPI);
    slavePinMode(SPI_SLAVE_1_SCK, PIN_MODE_SPI);
#endif
#ifdef INPUT_WT_SLAVE_NECK
    slaveInitWt();
#endif
}
#endif
int16_t adc_i(uint8_t pin) {
    int32_t ret = adc(pin);
    return ret - 32767;
}
int16_t handle_calibration_xbox(int16_t previous, int16_t orig_val, int16_t offset, int16_t min, int16_t multiplier, int16_t deadzone) {
    int32_t val = orig_val;
    int16_t val_deadzone = orig_val - offset;
    if (val_deadzone < deadzone && val_deadzone > -deadzone) {
        return 0;
    }
    if (val < 0) {
        deadzone = -deadzone;
    }
    val -= deadzone;
    val -= min;
    val *= multiplier;
    val /= 512;
    val += INT16_MIN;
    if (val > INT16_MAX) {
        val = INT16_MAX;
    }
    if (val < INT16_MIN) {
        val = INT16_MIN;
    }
    if (abs(val) > abs(previous)) {
        return val;
    }
    return previous;
}

int16_t handle_calibration_xbox_whammy(int16_t previous, uint16_t orig_val, uint16_t min, int16_t multiplier, uint16_t deadzone) {
    int32_t val = orig_val;
    if (multiplier > 0) {
        if ((val - min) < deadzone) {
            return INT16_MIN;
        }
    } else {
        if (val > min) {
            return INT16_MIN;
        }
    }
    val -= min;
    val *= multiplier;
    val /= 512;
    val -= INT16_MAX;
    if (val > INT16_MAX) {
        val = INT16_MAX;
    }
    if (val < INT16_MIN) {
        val = INT16_MIN;
    }
    if (val > previous) {
        return val;
    }
    return previous;
}
uint16_t handle_calibration_xbox_one_trigger(uint16_t previous, uint16_t orig_val, uint16_t min, int16_t multiplier, uint16_t deadzone) {
    int32_t val = orig_val;
    if (multiplier > 0) {
        if ((val - min) < deadzone) {
            return 0;
        }
    } else {
        if (val > min) {
            return 0;
        }
    }
    val -= min;
    val *= multiplier;
    val /= 512;
    if (val > UINT16_MAX) {
        val = UINT16_MAX;
    }
    if (val < 0) {
        val = 0;
    }
    val = val >> 6;
    if (val > previous) {
        return val;
    }
    return previous;
}
uint16_t handle_calibration_drum(uint16_t previous, uint16_t orig_val, uint16_t min, int16_t multiplier, uint16_t deadzone) {
    int32_t val = orig_val;
    if (multiplier > 0) {
        if ((val - min) < deadzone) {
            return 0;
        }
    } else {
        if (val > min) {
            return 0;
        }
    }
    val -= min;
    val *= multiplier;
    val /= 512;
    if (val > UINT16_MAX) {
        val = UINT16_MAX;
    }
    if (val < 0) {
        val = 0;
    }
    if (val > previous) {
        return val;
    }
    return previous;
}
uint8_t handle_calibration_ps3(uint8_t previous, int16_t orig_val, int16_t offset, int16_t min, int16_t multiplier, int16_t deadzone) {
    int8_t ret = handle_calibration_xbox((previous - PS3_STICK_CENTER) << 8, orig_val, offset, min, multiplier, deadzone) >> 8;
    return (uint8_t)(ret + PS3_STICK_CENTER);
}

int8_t handle_calibration_mouse(int8_t previous, int16_t orig_val, int16_t offset, int16_t min, int16_t multiplier, int16_t deadzone) {
    return handle_calibration_xbox(previous << 8, orig_val, offset, min, multiplier, deadzone) >> 8;
}

uint8_t handle_calibration_ps3_360_trigger(uint8_t previous, uint16_t orig_val, uint16_t min, int16_t multiplier, uint16_t deadzone) {
    return handle_calibration_xbox_one_trigger(previous << 2, orig_val, min, multiplier, deadzone) >> 2;
}

uint16_t handle_calibration_ps3_accel(uint16_t previous, uint16_t orig_val, int16_t offset, uint16_t min, int16_t multiplier, uint16_t deadzone) {
#if DEVICE_TYPE_IS_GUITAR || DEVICE_TYPE_IS_LIVE_GUITAR
    // For whatever reason, the acceleration for guitars swings between -128 to 128, not -512 to 512
    // Also, the game is looking for the value going negative, not positive
    int16_t ret = (-(handle_calibration_xbox((-(previous + GUITAR_ONE_G)) << 7, orig_val, offset, min, multiplier, deadzone) >> 7)) - GUITAR_ONE_G;
#else
    int16_t ret = handle_calibration_xbox((previous - PS3_ACCEL_CENTER) << 6, orig_val, offset, min, multiplier, deadzone) >> 6;
#endif
    return PS3_ACCEL_CENTER + ret;
}
long last_zero = 0;
uint8_t handle_calibration_ps3_whammy(uint8_t previous, uint16_t orig_val, uint16_t min, int16_t multiplier, uint16_t deadzone) {
#if DEVICE_TYPE == ROCK_BAND_GUITAR
    int8_t ret = handle_calibration_xbox_whammy((previous - PS3_STICK_CENTER) << 8, orig_val, min, multiplier, deadzone) >> 8;
    if (ret < -120) {
        if (last_zero - millis() > 1000) {
            return PS3_STICK_CENTER;
        }
    } else {
        last_zero = millis() + 1000;
    }
    return (uint8_t)(ret + PS3_STICK_CENTER);
#else
    int8_t ret = handle_calibration_xbox_whammy((previous - (PS3_STICK_CENTER + (PS3_STICK_CENTER >> 1))) << 9, orig_val, min, multiplier, deadzone) >> 9;
    return (uint8_t)(ret + PS3_STICK_CENTER + (PS3_STICK_CENTER >> 1));
#endif
}

uint8_t handle_calibration_turntable_ps3(uint8_t previous, int16_t orig_val, uint8_t multiplier) {
    int32_t val = (orig_val * multiplier) >> 8;
    if (val > INT8_MAX) {
        val = INT8_MAX;
    }
    if (val < INT8_MIN) {
        val = INT8_MIN;
    }
    // previous is uint, convert to int to test
    if (abs(val) > abs((int)previous - PS3_STICK_CENTER)) {
        // ps3 wants uint, convert to uint
        return (uint8_t)(val + PS3_STICK_CENTER);
    }
    return previous;
}
int16_t handle_calibration_turntable_360(int16_t previous, int16_t orig_val, uint8_t multiplier) {
    int32_t val = (orig_val * multiplier) >> 9;
    if (val > 64) {
        val = 64;
    }
    if (val < -64) {
        val = -64;
    }
    if (abs(val) > abs(previous)) {
        return (int16_t)val;
    }
    return previous;
}
uint16_t descriptor_index = 0;
uint8_t tick_xbox_one() {
    uint32_t serial = micros();
    switch (xbox_one_state) {
        case Announce:
            descriptor_index = 0;
            xbox_one_state = WaitingDesc1;
            memcpy(&combined_report, announce, sizeof(announce));
            memcpy(&combined_report.raw[7], &serial, 3);
            return sizeof(announce);
        case IdentDesc1:
            xbox_one_state = WaitingDesc;
            descriptor_index += 64;
            memcpy(&combined_report, xb1_descriptor, 64);
            return 64;
        case IdentDesc: {
            uint16_t len = sizeof(xb1_descriptor) - descriptor_index;
            if (len > 64) {
                len = 64;
            } else {
                xbox_one_state = WaitingDescEnd;
            }
            memcpy(&combined_report, xb1_descriptor + descriptor_index, len);
            descriptor_index += len;
            return len;
        }
        case IdentDescEnd: {
            xbox_one_state = Auth;
            memcpy(&combined_report, xb1_descriptor_end, sizeof(xb1_descriptor_end));
            return sizeof(xb1_descriptor_end);
        }
        case Auth:
            if (data_from_controller_size) {
                uint8_t size = data_from_controller_size;
                data_from_controller_size = 0;
                memcpy(&combined_report, data_from_controller, size);
                return size;
            }
            return 0;
        case Ready:
            return 0;
        default:
            return 0;
    }
}

long lastTick;
uint8_t keyboard_report = 0;
#if defined(BLUETOOTH_RX) && DEVICE_TYPE_IS_NORMAL_GAMEPAD
// When we do Bluetooth, the reports are in universal format, so we need to convert
void convert_universal_to_type(uint8_t *buf, PC_REPORT *report, uint8_t output_console_type) {
    uint8_t dpad = report->dpad >= 0x08 ? 0 : dpad_bindings_reverse[report->dpad];
    bool up = dpad & UP;
    bool left = dpad & LEFT;
    bool down = dpad & DOWN;
    bool right = dpad & RIGHT;
#if DEVICE_TYPE == GUITAR_HERO_GUITAR
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = report->tilt;
        }
        if (report->whammy) {
            out->whammy = (report->whammy - PS3_STICK_CENTER) << 1;
            ;
        }
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = (report->tilt - PS3_STICK_CENTER) << 8;
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = (report->whammy - PS3_STICK_CENTER) << 8;
        }
        // There is a specific mechanism for converting from the ps3 to the xbox 360 slider
        if (report->slider) {
            out->slider = report->slider;
            if (report->slider > 0x80) {
                out->slider |= (report->slider - 1) << 8;
            } else {
                out->slider |= (report->slider) << 8;
            }
        }
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = report->tilt;
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = report->whammy;
        }
        if (report->slider) {
            out->slider = report->slider;
        }
    }
#elif DEVICE_TYPE == ROCK_BAND_GUITAR
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->a |= report->a || report->soloGreen;
        out->b |= report->b || report->soloRed;
        out->y |= report->y || report->soloYellow;
        out->x |= report->x || report->soloBlue;
        out->leftShoulder |= report->leftShoulder || report->soloOrange;

        out->soloGreen |= report->soloGreen;
        out->soloRed |= report->soloRed;
        out->soloYellow |= report->soloYellow;
        out->soloBlue |= report->soloBlue;
        out->soloOrange |= report->soloOrange;

        out->green |= report->a;
        out->red |= report->b;
        out->yellow |= report->y;
        out->blue |= report->x;
        out->orange |= report->leftShoulder;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->solo |= report->soloGreen || report->soloRed || report->soloYellow || report->soloBlue || report->soloOrange;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = report->tilt;
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = report->whammy;
        }
        if (report->pickup != PS3_STICK_CENTER) {
            out->pickup = report->pickup;
        }
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->a |= report->a || report->soloGreen;
        out->b |= report->b || report->soloRed;
        out->y |= report->y || report->soloYellow;
        out->x |= report->x || report->soloBlue;
        out->leftShoulder |= report->leftShoulder || report->soloOrange;
        out->solo |= report->soloGreen || report->soloRed || report->soloYellow || report->soloBlue || report->soloOrange;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = (report->tilt - PS3_STICK_CENTER) << 8;
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = (report->whammy - PS3_STICK_CENTER) << 8;
        }
        if (report->pickup != PS3_STICK_CENTER) {
            out->pickup = (report->pickup) << 8;
        }
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->a |= report->a || report->soloGreen;
        out->b |= report->b || report->soloRed;
        out->y |= report->y || report->soloYellow;
        out->x |= report->x || report->soloBlue;
        out->leftShoulder |= report->leftShoulder || report->soloOrange;
        out->solo |= report->soloGreen || report->soloRed || report->soloYellow || report->soloBlue || report->soloOrange;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = report->tilt << 2;
            // TODO: this
            out->tiltDigital = true;
        }
        if (report->whammy) {
            last_zero = millis() + 1000;
            out->whammy = report->whammy;
        } else if (last_zero - millis() > 1000) {
            out->whammy = PS3_STICK_CENTER;
        }
        if (report->pickup != PS3_STICK_CENTER) {
            out->pickup = report->pickup;
        }
    }
#elif DEVICE_TYPE == LIVE_GUITAR
    if (output_console_type == PS4) {
        PS4_REPORT *out = (PS4_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->leftShoulder = report->leftShoulder;
        out->rightShoulder = report->rightShoulder;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;
        out->leftThumbClick |= report->leftThumbClick;

        out->guide |= report->guide;
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->leftShoulder = report->leftShoulder;
        out->rightShoulder = report->rightShoulder;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        out->leftThumbClick |= report->leftThumbClick;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = (report->tilt - PS3_STICK_CENTER) << 8;
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = (report->whammy - PS3_STICK_CENTER) << 1;
        }
        if (up) {
            out->strumBar = INT16_MIN;
        } else if (down) {
            out->strumBar = INT16_MAX;
        } else {
            out->strumBar = 0;
        }
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->leftShoulder = report->leftShoulder;
        out->rightShoulder = report->rightShoulder;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        out->leftThumbClick |= report->leftThumbClick;
        if (report->tilt != PS3_STICK_CENTER) {
            out->tilt = report->tilt << 2;
            if (report->tilt >= 0xF0) {
                out->tilt2 = 0xFF;
            }
            if (report->tilt <= 0x10) {
                out->tilt2 = 0x00;
            }
        }
        if (report->whammy != PS3_STICK_CENTER) {
            out->whammy = (report->whammy - PS3_STICK_CENTER) << 1;
        }
        if (up) {
            out->strumBar = 0x00;
        } else if (down) {
            out->strumBar = 0xFF;
        } else {
            out->strumBar = PS3_STICK_CENTER;
        }
    }
#elif DEVICE_TYPE == GUITAR_HERO_DRUMS
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = report->yellowVelocity >> 4;
        }
        if (report->redVelocity) {
            out->redVelocity = report->redVelocity >> 4;
        }
        if (report->greenVelocity) {
            out->greenVelocity = report->greenVelocity >> 4;
        }
        if (report->blueVelocity) {
            out->blueVelocity = report->blueVelocity >> 4;
        }
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = report->yellowVelocity;
        }
        if (report->redVelocity) {
            out->redVelocity = report->redVelocity;
        }
        if (report->greenVelocity) {
            out->greenVelocity = report->greenVelocity;
        }
        if (report->blueVelocity) {
            out->blueVelocity = report->blueVelocity;
        }
        if (report->kickVelocity) {
            out->kickVelocity = report->kickVelocity;
        }
        if (report->orangeVelocity) {
            out->orangeVelocity = report->orangeVelocity;
        }
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = report->yellowVelocity;
        }
        if (report->redVelocity) {
            out->redVelocity = report->redVelocity;
        }
        if (report->greenVelocity) {
            out->greenVelocity = report->greenVelocity;
        }
        if (report->blueVelocity) {
            out->blueVelocity = report->blueVelocity;
        }
        if (report->kickVelocity) {
            out->kickVelocity = report->kickVelocity;
        }
        if (report->orangeVelocity) {
            out->orangeVelocity = report->orangeVelocity;
        }
    }
#elif DEVICE_TYPE == ROCK_BAND_DRUMS
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = report->yellowVelocity >> 4;
        }
        if (report->redVelocity) {
            out->redVelocity = report->redVelocity >> 4;
        }
        if (report->greenVelocity) {
            out->greenVelocity = report->greenVelocity >> 4;
        }
        if (report->blueCymbalVelocity) {
            out->blueCymbalVelocity = report->blueCymbalVelocity >> 4;
        }
        if (report->yellowCymbalVelocity) {
            out->yellowCymbalVelocity = report->yellowCymbalVelocity >> 4;
        }
        if (report->greenCymbalVelocity) {
            out->greenCymbalVelocity = report->greenCymbalVelocity >> 4;
        }
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;
        out->padFlag = report->padFlag;
        out->cymbalFlag = report->cymbalFlag;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = -(0x7FFF - (report->yellowVelocity << 7));
        }
        if (report->redVelocity) {
            out->redVelocity = (0x7FFF - (report->redVelocity << 7));
        }
        if (report->greenVelocity) {
            out->greenVelocity = -(0x7FFF - (report->greenVelocity << 7));
        }
        if (report->blueVelocity) {
            out->blueVelocity = (0x7FFF - (report->blueVelocity << 7));
        }
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;
        out->padFlag = report->padFlag;
        out->cymbalFlag = report->cymbalFlag;

        out->guide |= report->guide;
        if (report->yellowVelocity) {
            out->yellowVelocity = report->yellowVelocity;
        }
        if (report->redVelocity) {
            out->redVelocity = report->redVelocity;
        }
        if (report->greenVelocity) {
            out->greenVelocity = report->greenVelocity;
        }
        if (report->blueVelocity) {
            out->blueVelocity = report->blueVelocity;
        }
    }
#elif DEVICE_TYPE == DJ_HERO_TURNTABLE
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        if (report->leftTableVelocity != PS3_STICK_CENTER) {
            out->leftTableVelocity = (report->leftTableVelocity - PS3_STICK_CENTER) << 8;
        }
        if (report->rightTableVelocity != PS3_STICK_CENTER) {
            out->rightTableVelocity = (report->rightTableVelocity - PS3_STICK_CENTER) << 8;
        }
        if (report->effectsKnob != PS3_STICK_CENTER) {
            out->effectsKnob = (report->effectsKnob - PS3_STICK_CENTER) << 8;
        }
        if (report->crossfader != PS3_STICK_CENTER) {
            out->crossfader = (report->crossfader - PS3_STICK_CENTER) << 8;
        }
        out->leftBlue |= report->leftBlue;
        out->leftRed |= report->leftRed;
        out->leftGreen |= report->leftGreen;
        out->rightBlue |= report->rightBlue;
        out->rightRed |= report->rightRed;
        out->rightGreen |= report->rightGreen;
    }
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        if (report->leftTableVelocity != PS3_STICK_CENTER) {
            out->leftTableVelocity = report->leftTableVelocity;
        }
        if (report->rightTableVelocity != PS3_STICK_CENTER) {
            out->rightTableVelocity = report->rightTableVelocity;
        }
        if (report->effectsKnob != PS3_STICK_CENTER) {
            out->effectsKnob = report->effectsKnob << 2;
        }
        if (report->crossfader != PS3_STICK_CENTER) {
            out->crossfader = report->crossfader << 2;
        }
        out->leftBlue |= report->leftBlue;
        out->leftRed |= report->leftRed;
        out->leftGreen |= report->leftGreen;
        out->rightBlue |= report->rightBlue;
        out->rightRed |= report->rightRed;
        out->rightGreen |= report->rightGreen;
    }
#elif DEVICE_TYPE == STAGE_KIT
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        out->capture |= report->capture;
    }
    if (output_console_type == PS4) {
        PS4_REPORT *out = (PS4_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        out->capture |= report->capture;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
    }
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
    }
#else  // Gamepad or devices that use the same mapping as a gamepad
    if (output_console_type == PS3) {
        PS3_REPORT *out = (PS3_REPORT *)buf;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;
        out->l2 |= report->l2;
        out->r2 |= report->r2;

        out->back |= report->back;
        out->start |= report->start;
        out->leftThumbClick |= report->leftThumbClick;
        out->rightThumbClick |= report->rightThumbClick;

        out->guide |= report->guide;
        if (report->leftStickX != PS3_STICK_CENTER) {
            out->leftStickX = report->leftStickX;
        }
        if (report->leftStickY != PS3_STICK_CENTER) {
            out->leftStickY = report->leftStickY;
        }
        if (report->rightStickX != PS3_STICK_CENTER) {
            out->rightStickX = report->rightStickX;
        }
        if (report->rightStickY != PS3_STICK_CENTER) {
            out->rightStickY = report->rightStickY;
        }
        if (report->leftTrigger) {
            out->leftTrigger = report->leftTrigger;
        }
        if (report->rightTrigger) {
            out->rightTrigger = report->rightTrigger;
        }
        if (report->pressureDpadUp) {
            out->pressureDpadUp = report->pressureDpadUp;
        }
        if (report->pressureDpadRight) {
            out->pressureDpadRight = report->pressureDpadRight;
        }
        if (report->pressureDpadDown) {
            out->pressureDpadDown = report->pressureDpadDown;
        }
        if (report->pressureDpadLeft) {
            out->pressureDpadLeft = report->pressureDpadLeft;
        }
        if (report->pressureL1) {
            out->pressureL1 = report->pressureL1;
        }
        if (report->pressureR1) {
            out->pressureR1 = report->pressureR1;
        }
        if (report->pressureTriangle) {
            out->pressureTriangle = report->pressureTriangle;
        }
        if (report->pressureCircle) {
            out->pressureCircle = report->pressureCircle;
        }
        if (report->pressureCross) {
            out->pressureCross = report->pressureCross;
        }
        if (report->pressureSquare) {
            out->pressureSquare = report->pressureSquare;
        }
    }
    if (output_console_type == REAL_PS3) {
        PS3Gamepad_Data_t *out = (PS3Gamepad_Data_t *)buf;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;
        out->l2 |= report->l2;
        out->r2 |= report->r2;

        out->back |= report->back;
        out->start |= report->start;
        out->leftThumbClick |= report->leftThumbClick;
        out->rightThumbClick |= report->rightThumbClick;

        out->guide |= report->guide;
        if (report->leftStickX != PS3_STICK_CENTER) {
            out->leftStickX = report->leftStickX;
        }
        if (report->leftStickY != PS3_STICK_CENTER) {
            out->leftStickY = report->leftStickY;
        }
        if (report->rightStickX != PS3_STICK_CENTER) {
            out->rightStickX = report->rightStickX;
        }
        if (report->rightStickY != PS3_STICK_CENTER) {
            out->rightStickY = report->rightStickY;
        }
        if (report->leftTrigger) {
            out->leftTrigger = report->leftTrigger;
        }
        if (report->rightTrigger) {
            out->rightTrigger = report->rightTrigger;
        }
        if (report->pressureDpadUp) {
            out->pressureDpadUp = report->pressureDpadUp;
        }
        if (report->pressureDpadRight) {
            out->pressureDpadRight = report->pressureDpadRight;
        }
        if (report->pressureDpadDown) {
            out->pressureDpadDown = report->pressureDpadDown;
        }
        if (report->pressureDpadLeft) {
            out->pressureDpadLeft = report->pressureDpadLeft;
        }
        if (report->pressureL1) {
            out->pressureL1 = report->pressureL1;
        }
        if (report->pressureR1) {
            out->pressureR1 = report->pressureR1;
        }
        if (report->pressureTriangle) {
            out->pressureTriangle = report->pressureTriangle;
        }
        if (report->pressureCircle) {
            out->pressureCircle = report->pressureCircle;
        }
        if (report->pressureCross) {
            out->pressureCross = report->pressureCross;
        }
        if (report->pressureSquare) {
            out->pressureSquare = report->pressureSquare;
        }
    }
    if (output_console_type == PS4) {
        PS4_REPORT *out = (PS4_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;
        out->l2 |= report->l2;
        out->r2 |= report->r2;

        out->back |= report->back;
        out->start |= report->start;
        out->leftThumbClick |= report->leftThumbClick;
        out->rightThumbClick |= report->rightThumbClick;

        out->guide |= report->guide;
        out->capture |= report->capture;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        if (report->leftStickX != PS3_STICK_CENTER) {
            out->leftStickX = report->leftStickX;
        }
        if (report->leftStickY != PS3_STICK_CENTER) {
            out->leftStickY = report->leftStickY;
        }
        if (report->rightStickX != PS3_STICK_CENTER) {
            out->rightStickX = report->rightStickX;
        }
        if (report->rightStickY != PS3_STICK_CENTER) {
            out->rightStickY = report->rightStickY;
        }
        if (report->leftTrigger) {
            out->leftTrigger = report->leftTrigger;
        }
        if (report->rightTrigger) {
            out->rightTrigger = report->rightTrigger;
        }
    }
    if (output_console_type == XBOXONE) {
        XBOX_ONE_REPORT *out = (XBOX_ONE_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;

        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;
        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;

        out->back |= report->back;
        out->start |= report->start;
        out->leftThumbClick |= report->leftThumbClick;
        out->rightThumbClick |= report->rightThumbClick;

        out->guide |= report->guide;
        if (report->leftStickX != PS3_STICK_CENTER) {
            out->leftStickX = (report->leftStickX - 128) << 8;
        }
        if (report->leftStickY != PS3_STICK_CENTER) {
            out->leftStickY = (report->leftStickY - 128) << 8;
        }
        if (report->rightStickX != PS3_STICK_CENTER) {
            out->rightStickX = (report->rightStickX - 128) << 8;
        }
        if (report->rightStickY != PS3_STICK_CENTER) {
            out->rightStickY = (report->rightStickY - 128) << 8;
        }
        if (report->leftTrigger) {
            out->leftTrigger = (report->leftTrigger) << 8;
        }
        if (report->rightTrigger) {
            out->rightTrigger = (report->rightTrigger) << 8;
        }
    }
    if (output_console_type == XBOX360) {
        XINPUT_REPORT *out = (XINPUT_REPORT *)buf;
        out->x |= report->x;
        out->a |= report->a;
        out->b |= report->b;
        out->y |= report->y;
        out->dpadUp |= up;
        out->dpadDown |= down;
        out->dpadLeft |= left;
        out->dpadRight |= right;

        out->back |= report->back;
        out->start |= report->start;

        out->guide |= report->guide;
        if (report->leftStickX != PS3_STICK_CENTER) {
            out->leftStickX = (report->leftStickX - 128) << 8;
        }
        if (report->leftStickY != PS3_STICK_CENTER) {
            out->leftStickY = (report->leftStickY - 128) << 8;
        }
        if (report->rightStickX != PS3_STICK_CENTER) {
            out->rightStickX = (report->rightStickX - 128) << 8;
        }
        if (report->rightStickY != PS3_STICK_CENTER) {
            out->rightStickY = (report->rightStickY - 128) << 8;
        }
        if (report->leftTrigger) {
            out->leftTrigger = report->leftTrigger;
        }
        if (report->rightTrigger) {
            out->rightTrigger = report->rightTrigger;
        }

        out->leftShoulder |= report->leftShoulder;
        out->rightShoulder |= report->rightShoulder;
        out->leftThumbClick |= report->leftThumbClick;
        out->rightThumbClick |= report->rightThumbClick;
    }
#endif
}
#endif
uint8_t tick_inputs(void *buf, USB_LastReport_Data_t *last_report, uint8_t output_console_type) {
    uint8_t size = 0;
    uint8_t led_tmp;
    Buffer_Report_t current_queue_report = {val : 0};
// Tick Inputs
#include "inputs/clone_neck.h"
#include "inputs/gh5_neck.h"
#include "inputs/ps2.h"
#include "inputs/slave_tick.h"
#include "inputs/turntable.h"
#include "inputs/usb_host.h"
#include "inputs/wii.h"
#include "inputs/wt_neck.h"
    TICK_SHARED;
    // give the user 5 seconds to jump between modes (aka, hold on plug in)
    if (millis() < 5000 && (output_console_type == UNIVERSAL || output_console_type == WINDOWS)) {
        TICK_DETECTION;
    }
    // We tick the guitar every 5ms to handle inputs if nothing is attempting to read, but this doesn't need to output that data anywhere.
    // if input queues are enabled, then we just tick as often as possible
    if (!buf) {
        if (INPUT_QUEUE) {
            if (micros() - last_queue > 100) {
                last_queue = micros();
                for (int i = 0; i < DIGITAL_COUNT; i++) {
                    if (debounce[i]) {
                        debounce[i]--;
                    }
                }
            }
            if (current_queue_report.val != last_queue_report.val) {
                queue[queue_tail] = current_queue_report;
                last_queue_report = current_queue_report;
                if (queue_size < BUFFER_SIZE_QUEUE) {
                    queue_size++;
                    queue_tail++;
                }
            }
        }
        return 0;
    }

    if (INPUT_QUEUE && queue_size) {
        current_queue_report = queue[queue_tail - queue_size];
        queue_size--;
    }
    // Tick all three reports, and then go for the first one that has changes
    // We prioritise NKRO, then Consumer, because these are both only buttons
    // Then mouse, as it is an axis so it is more likley to have changes
#if DEVICE_TYPE_IS_KEYBOARD
    void *lastReportToCheck;
    for (int i = 1; i < REPORT_ID_END; i++) {
#ifdef TICK_MOUSE
        if (i == REPORT_ID_MOUSE) {
            size = sizeof(USB_Mouse_Data_t);
            memset(buf, 0, size);
            USB_Mouse_Data_t *report = (USB_Mouse_Data_t *)buf;
            report->rid = REPORT_ID_MOUSE;
            TICK_MOUSE;
            if (last_report) {
                lastReportToCheck = &last_report->lastMouseReport;
            }
        }
#endif
#ifdef TICK_CONSUMER
        if (i == REPORT_ID_CONSUMER) {
            size = sizeof(USB_ConsumerControl_Data_t);
            memset(buf, 0, size);
            USB_ConsumerControl_Data_t *report = (USB_ConsumerControl_Data_t *)buf;
            report->rid = REPORT_ID_CONSUMER;
            TICK_CONSUMER;
            if (last_report) {
                lastReportToCheck = &last_report->lastConsumerReport;
            }
        }
#endif
#ifdef TICK_NKRO
        if (i == REPORT_ID_NKRO) {
            size = sizeof(USB_NKRO_Data_t);
            memset(buf, 0, size);
            USB_NKRO_Data_t *report = (USB_NKRO_Data_t *)buf;
            report->rid = REPORT_ID_NKRO;
            TICK_NKRO;
            if (last_report) {
                lastReportToCheck = &last_report->lastNKROReport;
            }
        }
#endif
#ifdef TICK_SIXKRO
        if (i == REPORT_ID_NKRO) {
            size = sizeof(USB_6KRO_Data_t);
            memset(buf, 0, size);
            USB_6KRO_Data_t *report = (USB_6KRO_Data_t *)buf;
            report->rid = REPORT_ID_NKRO;
            TICK_SIXKRO;
            if (last_report) {
                lastReportToCheck = &last_report->last6KROReport;
            }
        }
#endif

        // If we are directly asked for a HID report, always just reply with the NKRO one
        if (lastReportToCheck) {
            uint8_t cmp = memcmp(lastReportToCheck, buf, size);
            if (cmp == 0) {
                size = 0;
                continue;
            }
            memcpy(lastReportToCheck, buf, size);
            break;
        } else {
            break;
        }
    }
#else
    USB_Report_Data_t *report_data = (USB_Report_Data_t *)buf;
    uint8_t report_size;
    bool updateSequence = false;
    bool updateHIDSequence = false;
#if USB_HOST_STACK
    if (output_console_type == XBOXONE) {
        // The GHL guitar is special. It uses a standard nav report in the xbox menus, but then in game, it uses the ps3 report.
        // To switch modes, a poke command is sent every 8 seconds
        // In nav mode, we handle things like a controller, while in ps3 mode, we fall through and just set the report using ps3 mode.

        if (!DEVICE_TYPE_IS_LIVE_GUITAR || millis() - last_ghl_poke_time < 8000) {
            XBOX_ONE_REPORT *report = (XBOX_ONE_REPORT *)buf;
            size = sizeof(XBOX_ONE_REPORT);
            report_size = size - sizeof(GipHeader_t);
            memset(buf, 0, size);
            GIP_HEADER(report, GIP_INPUT_REPORT, false, report_sequence_number);
            int16_t temp;
            uint16_t temp_trigger;
            TICK_XBOX_ONE;
            #if DEVICE_TYPE_IS_GUITAR
            if (report->tilt < 0x70) {
                report->tilt = 0;
            }
            #endif

            if (report->guide != lastXboxOneGuide) {
                lastXboxOneGuide = report->guide;
                GipKeystroke_t *keystroke = (GipKeystroke_t *)buf;
                GIP_HEADER(keystroke, GIP_VIRTUAL_KEYCODE, true, keystroke_sequence_number++);
                keystroke->pressed = report->guide;
                keystroke->keycode = GIP_VKEY_LEFT_WIN;
                return sizeof(GipKeystroke_t);
            }
            // We use an unused bit as a flag for sending the guide key code, so flip it back
            report->guide = false;
            GipPacket_t *packet = (GipPacket_t *)buf;
            report_data = (USB_Report_Data_t *)packet->data;
            updateSequence = true;
        } else {
            XboxOneGHLGuitar_Data_t *report = (XboxOneGHLGuitar_Data_t *)buf;
            size = sizeof(XboxOneGHLGuitar_Data_t);
            report_size = sizeof(PS3_REPORT);
            memset(buf, 0, sizeof(XboxOneGHLGuitar_Data_t));
            GIP_HEADER(report, GHL_HID_REPORT, false, hid_sequence_number);
            report_data = (USB_Report_Data_t *)&report->report;
            updateHIDSequence = true;
        }
    }
#endif
    if (output_console_type == WINDOWS || output_console_type == XBOX360) {
        XINPUT_REPORT *report = (XINPUT_REPORT *)report_data;
        memset(report_data, 0, sizeof(XINPUT_REPORT));
        report->rid = 0;
        report->rsize = sizeof(XINPUT_REPORT);
// Whammy on the xbox guitars goes from min to max, so it needs to default to min
#if DEVICE_TYPE_IS_GUITAR || DEVICE_TYPE_IS_LIVE_GUITAR
        report->whammy = INT16_MIN;
#endif
        TICK_XINPUT;

// xb360 is stupid
#if DEVICE_TYPE == GUITAR_HERO_DRUMS
        report->leftThumbClick = true;
#endif
        report_size = size = sizeof(XINPUT_REPORT);
    }
// Guitars and Drums can fall back to their PS3 versions, so don't even include the PS4 version there.
// DJ Hero was never on ps4, so we can't really implement that either, so just fall back to PS3 there too.
#if SUPPORTS_PS4
    if (output_console_type == PS4) {
        if (millis() > 450000 && !auth_ps4_controller_found) {
            reset_usb();
        }
        PS4_REPORT *report = (PS4_REPORT *)report_data;
        memset(report, 0, sizeof(PS4_REPORT));
        PS4Dpad_Data_t *gamepad = (PS4Dpad_Data_t *)report;
        gamepad->report_id = 0x01;
        gamepad->leftStickX = PS3_STICK_CENTER;
        gamepad->leftStickY = PS3_STICK_CENTER;
        gamepad->rightStickX = PS3_STICK_CENTER;
        gamepad->rightStickY = PS3_STICK_CENTER;
        // PS4 does not start using the controller until it sees a PS button press.
        if (!seen_ps4_console) {
            report->guide = true;
        }
        TICK_PS4;
        asm volatile("" ::
                         : "memory");
        gamepad->dpad = (gamepad->dpad & 0xf) > 0x0a ? 0x08 : dpad_bindings[gamepad->dpad];
        report_size = size = sizeof(PS4_REPORT);
    }
#endif
    if (output_console_type == BLUETOOTH_REPORT || output_console_type == UNIVERSAL) {
        report_size = size = sizeof(PC_REPORT);
        PC_REPORT *report = (PC_REPORT *)report_data;
        memset(report, 0, sizeof(PC_REPORT));
#if DEVICE_TYPE == GAMEPAD
        report->leftStickX = PS3_STICK_CENTER;
        report->leftStickY = PS3_STICK_CENTER;
        report->rightStickX = PS3_STICK_CENTER;
        report->rightStickY = PS3_STICK_CENTER;
#endif
#if DEVICE_TYPE_IS_GUITAR || DEVICE_TYPE_IS_LIVE_GUITAR
        report->tilt = PS3_STICK_CENTER;
#endif
#if DEVICE_TYPE == DJ_HERO_TURNTABLE
        report->leftTableVelocity = PS3_STICK_CENTER;
        report->rightTableVelocity = PS3_STICK_CENTER;
        report->crossfader = PS3_STICK_CENTER;
        report->effectsKnob = PS3_STICK_CENTER;
#endif
        report->reportId = 1;
        TICK_PC;
        asm volatile("" ::
                         : "memory");
        report->dpad = dpad_bindings[report->dpad];
    }
// If we are dealing with a non instrument controller (like a gamepad) then we use the proper ps3 controller report format, to allow for emulator support and things like that
// This also gives us PS2 support via PADEMU and wii support via fakemote for standard controllers.
// However, actual ps3 support was being a pain so we use the instrument descriptor there, since the ps3 doesn't care.
// This also has the bonus that it will keep switch support working as well
#if DEVICE_TYPE == GAMEPAD
    if (output_console_type == PS3) {
        PS3Gamepad_Data_t *report = (PS3Gamepad_Data_t *)report_data;
        memset(report, 0, sizeof(PS3Gamepad_Data_t));
        report->reportId = 1;
        report->accelX = PS3_ACCEL_CENTER;
        report->accelY = PS3_ACCEL_CENTER;
        report->accelZ = PS3_ACCEL_CENTER;
        report->gyro = PS3_ACCEL_CENTER;
        report->leftStickX = PS3_STICK_CENTER;
        report->leftStickY = PS3_STICK_CENTER;
        report->rightStickX = PS3_STICK_CENTER;
        report->rightStickY = PS3_STICK_CENTER;
        TICK_PS3_WITHOUT_CAPTURE;
        report_size = size = sizeof(PS3Gamepad_Data_t);
    }
    if (output_console_type != WINDOWS && output_console_type != XBOX360 && output_console_type != PS3 && output_console_type != BLUETOOTH_REPORT && output_console_type != UNIVERSAL && output_console_type != PS4 && !updateSequence) {
#else
    // For instruments, we instead use the below block, as all other console types use the below format
    if (output_console_type != WINDOWS && output_console_type != XBOX360 && output_console_type != PS4 && output_console_type != BLUETOOTH_REPORT && output_console_type != UNIVERSAL && !updateSequence) {
#endif
        report_size = size = sizeof(PS3_REPORT);
        PS3_REPORT *report = (PS3_REPORT *)report_data;
        memset(report, 0, sizeof(PS3_REPORT));
        PS3Dpad_Data_t *gamepad = (PS3Dpad_Data_t *)report_data;
        gamepad->accelX = PS3_ACCEL_CENTER;
        gamepad->accelY = PS3_ACCEL_CENTER;
        gamepad->accelZ = PS3_ACCEL_CENTER;
        gamepad->gyro = PS3_ACCEL_CENTER;
        gamepad->leftStickX = PS3_STICK_CENTER;
        gamepad->leftStickY = PS3_STICK_CENTER;
        gamepad->rightStickX = PS3_STICK_CENTER;
        gamepad->rightStickY = PS3_STICK_CENTER;
        TICK_PS3;
        asm volatile("" ::
                         : "memory");
        gamepad->dpad = dpad_bindings[gamepad->dpad];
#ifdef CONFIGURABLE_BLOBS
        if (SWAP_SWITCH_FACE_BUTTONS && output_console_type == SWITCH) {
            bool a = gamepad->a;
            bool b = gamepad->b;
            bool x = gamepad->x;
            bool y = gamepad->y;
            gamepad->b = a;
            gamepad->a = b;
            gamepad->x = y;
            gamepad->y = x;
        }
#elif SWAP_SWITCH_FACE_BUTTONS
        if (output_console_type == SWITCH) {
            bool a = gamepad->a;
            bool b = gamepad->b;
            bool x = gamepad->x;
            bool y = gamepad->y;
            gamepad->b = a;
            gamepad->a = b;
            gamepad->x = y;
            gamepad->y = x;
        }
#endif
    }

    TICK_RESET
    // Some hosts want packets sent every frame
    if (last_report && output_console_type != REAL_PS3 && output_console_type != PS4 && output_console_type != PS3 && output_console_type != BLUETOOTH_REPORT && output_console_type != XBOX360) {
        uint8_t cmp = memcmp(last_report, report_data, report_size);
        if (cmp == 0) {
            return 0;
        }
        memcpy(last_report, report_data, report_size);
    }
// Standard PS4 controllers need a report counter, but we don't want to include that when comparing so we add it here
#if SUPPORTS_PS4
    if (output_console_type == PS4) {
        PS4Gamepad_Data_t *gamepad = (PS4Gamepad_Data_t *)report_data;
        gamepad->reportCounter = ps4_sequence_number++;
    }
#endif
#if USB_HOST_STACK
#if DEVICE_TYPE_IS_NORMAL_GAMEPAD
    if (updateSequence) {
        report_sequence_number++;
        if (report_sequence_number == 0) {
            report_sequence_number = 1;
        }
    } else if (updateHIDSequence) {
        hid_sequence_number++;
        if (hid_sequence_number == 0) {
            hid_sequence_number = 1;
        }
    }
#endif
#endif
#endif
    return size;
}

#ifdef BLUETOOTH_TX
bool tick_bluetooth(void) {
    uint8_t size = tick_inputs(&bt_report, &last_report_bt, BLUETOOTH_REPORT);
    send_report(size, (uint8_t *)&bt_report);
    return size;
}
#endif
bool windows_in_hid = false;
unsigned long millis_at_boot = 0;
bool tick_usb(void) {
    uint8_t size = 0;
    bool ready = ready_for_next_packet();
#ifdef BLUETOOTH_TX
    if (!ready) {
        return false;
    }
#endif
    if (millis_at_boot == 0 && read_device_desc) {
        millis_at_boot = millis();
    }
#if !DEVICE_TYPE_IS_KEYBOARD
    if (!WINDOWS_USES_XINPUT) {
        // If we ended up here, then someone probably changed back to hid mode so we should jump back
        if (consoleType == WINDOWS) {
            consoleType = UNIVERSAL;
            reset_usb();
        }
    }
#endif
#if DEVICE_TYPE_IS_NORMAL_GAMEPAD
    // PS2 / Wii / WiiU do not read the hid report descriptor or any of the string descriptors.
    if (millis_at_boot && (millis() - millis_at_boot) > 5000 && consoleType == UNIVERSAL && !seen_hid_descriptor_read && !read_any_string && !seen_windows_xb1) {
        // The wii however will configure the usb device before it stops communicating
#if DEVICE_TYPE == ROCK_BAND_GUITAR || DEVICE_TYPE == ROCK_BAND_DRUMS
        if (usb_configured()) {
            set_console_type(WII_RB);
        }
#endif
        // But the PS2 does not. We also end up here on the wii/wiiu if a device does not have an explicit wii mode.
        set_console_type(PS3);
    }
    // Due to some quirks with how the PS3 detects controllers, we can also end up here for PS3, but in that case, we won't see any requests for controller data
    if ((millis() - millis_at_boot) > 2000 && consoleType == PS4 && !seen_ps4) {
        set_console_type(REAL_PS3);
    }
#endif
    if (!ready) return 0;
#if USB_HOST_STACK
    if (data_from_console_size) {
        send_report_to_controller(get_device_address_for(XBOXONE), data_from_console, data_from_console_size);
        data_from_console_size = 0;
    }
    // If we have something pending to send to the xbox one controller, send it
    if (consoleType == XBOXONE && xbox_one_state != Ready) {
        size = tick_xbox_one();
        if (!size) {
            return 0;
        }
    }
#endif
#ifndef BLUETOOTH_RX
    if (!size) {
        size = tick_inputs(&combined_report, &last_report_usb, consoleType);
        last_usb_report_size = size;
    }
#endif
    if (size) {
        send_report_to_pc(&combined_report, size);
    }
    seen_ps4_console = true;
    return size;
}
#ifdef BLUETOOTH_RX
int tick_bluetooth_inputs(const void *buf) {
    uint8_t size = 0;
    uint8_t report_size = 0;
    uint8_t output_console_type = consoleType;
    // Tick Inputs
#include "inputs/clone_neck.h"
#include "inputs/gh5_neck.h"
#include "inputs/ps2.h"
#include "inputs/slave_tick.h"
#include "inputs/turntable.h"
#include "inputs/usb_host.h"
#include "inputs/wii.h"
#include "inputs/wt_neck.h"
    TICK_SHARED;
#if DEVICE_TYPE_IS_KEYBOARD
#ifdef TICK_NKRO
    if (buf[0] == REPORT_ID_NKRO) {
        memcpy(&last_report_usb->lastNKROReport, buf, sizeof(last_report_usb->lastNKROReport));
        TICK_NKRO;
    }
#elif defined(TICK_CONSUMER)
    if (buf[0] == REPORT_ID_CONSUMER) {
        memcpy(&last_report_usb->lastConsumerReport, buf, sizeof(last_report_usb->lastConsumerReport));
        TICK_CONSUMER;
    }
#elif defined(TICK_MOUSE)
    if (buf[0] == REPORT_ID_MOUSE) {
        memcpy(&last_report_usb->lastMouseReport, buf, sizeof(last_report_usb->lastMouseReport));
        TICK_MOUSE;
    }
#endif

#else
    PC_REPORT *input = (PC_REPORT *)(buf);
    USB_Report_Data_t *report_data = &combined_report;
    if (output_console_type == UNIVERSAL) {
        report_size = size = sizeof(PC_REPORT);
        memcpy(report_data, buf, size);
        PC_REPORT *report = (PC_REPORT *)report_data;
        report->dpad = input->dpad >= 0x08 ? 0 : dpad_bindings_reverse[input->dpad];
        TICK_PC;
        asm volatile("" ::
                         : "memory");
        report->dpad = dpad_bindings[report->dpad];
    }
#if DEVICE_TYPE_IS_NORMAL_GAMEPAD
    bool updateSequence = false;
    bool updateHIDSequence = false;
    if (output_console_type == XBOXONE) {
        // The GHL guitar is special. It uses a standard nav report in the xbox menus, but then in game, it uses the ps3 report.
        // To switch modes, a poke command is sent every 8 seconds
        // In nav mode, we handle things like a controller, while in ps3 mode, we fall through and just set the report using ps3 mode.

        if (!DEVICE_TYPE_IS_LIVE_GUITAR || millis() - last_ghl_poke_time < 8000) {
            XBOX_ONE_REPORT *report = (XBOX_ONE_REPORT *)report_data;
            size = sizeof(XBOX_ONE_REPORT);
            report_size = size - sizeof(GipHeader_t);
            memset(report_data, 0, size);
            GIP_HEADER(report, GIP_INPUT_REPORT, false, report_sequence_number);
            convert_universal_to_type((uint8_t *)report_data, input, XBOXONE);
            TICK_XBOX_ONE;
            asm volatile("" ::
                             : "memory");
            if (report->guide != lastXboxOneGuide) {
                lastXboxOneGuide = report->guide;
                GipKeystroke_t *keystroke = (GipKeystroke_t *)report_data;
                GIP_HEADER(keystroke, GIP_VIRTUAL_KEYCODE, true, keystroke_sequence_number++);
                keystroke->pressed = report->guide;
                keystroke->keycode = GIP_VKEY_LEFT_WIN;
                return sizeof(GipKeystroke_t);
            }
            // We use an unused bit as a flag for sending the guide key code, so flip it back
            report->guide = false;
            GipPacket_t *packet = (GipPacket_t *)report_data;
            report_data = (USB_Report_Data_t *)packet->data;
            updateSequence = true;
        } else {
            XboxOneGHLGuitar_Data_t *report = (XboxOneGHLGuitar_Data_t *)report_data;
            size = sizeof(XboxOneGHLGuitar_Data_t);
            report_size = sizeof(PS3_REPORT);
            memset(report_data, 0, sizeof(XboxOneGHLGuitar_Data_t));
            GIP_HEADER(report, GHL_HID_REPORT, false, hid_sequence_number);
            asm volatile("" ::
                             : "memory");
            report_data = (USB_Report_Data_t *)&report->report;
            updateHIDSequence = true;
        }
    }
    if (output_console_type == WINDOWS || output_console_type == XBOX360) {
        XINPUT_REPORT *report = (XINPUT_REPORT *)report_data;
        memset(report_data, 0, sizeof(XINPUT_REPORT));
        report->rid = 0;
        report->rsize = sizeof(XINPUT_REPORT);
// Whammy on the xbox guitars goes from min to max, so it needs to default to min
#if DEVICE_TYPE_IS_GUITAR || DEVICE_TYPE_IS_LIVE_GUITAR
        report->whammy = INT16_MIN;
#endif
        convert_universal_to_type((uint8_t *)report_data, input, XBOX360);
        TICK_XINPUT;
        report_size = size = sizeof(XINPUT_REPORT);
    }
// Guitars and Drums can fall back to their PS3 versions, so don't even include the PS4 version there.
// DJ Hero was never on ps4, so we can't really implement that either, so just fall back to PS3 there too.
#if SUPPORTS_PS4
    if (output_console_type == PS4) {
        PS4_REPORT *report = (PS4_REPORT *)report_data;
        PS4Dpad_Data_t *gamepad = (PS4Dpad_Data_t *)report;
        report_size = size = sizeof(PS4_REPORT);
        memset(report_data, 0, size);
        gamepad->report_id = 0x01;
        gamepad->leftStickX = PS3_STICK_CENTER;
        gamepad->leftStickY = PS3_STICK_CENTER;
        gamepad->rightStickX = PS3_STICK_CENTER;
        gamepad->rightStickY = PS3_STICK_CENTER;
#if !DEVICE_TYPE_IS_LIVE_GUITAR
        gamepad->reportCounter = ps4_sequence_number;
#endif

        convert_universal_to_type((uint8_t *)report_data, input, output_console_type);
        TICK_PS4;
        asm volatile("" ::
                         : "memory");
        gamepad->dpad = dpad_bindings[gamepad->dpad];
    }
#endif
// If we are dealing with a non instrument controller (like a gamepad) then we use the proper ps3 controller report format, to allow for emulator support and things like that
// This also gives us PS2 support via PADEMU and wii support via fakemote for standard controllers.
// However, actual ps3 support was being a pain so we use the instrument descriptor there, since the ps3 doesn't care.
#if (DEVICE_TYPE == GAMEPAD)
    if (output_console_type == PS3) {
        PS3Gamepad_Data_t *report = (PS3Gamepad_Data_t *)report_data;
        report->accelX = PS3_ACCEL_CENTER;
        report->accelY = PS3_ACCEL_CENTER;
        report->accelZ = PS3_ACCEL_CENTER;
        report->gyro = PS3_ACCEL_CENTER;
        report->leftStickX = PS3_STICK_CENTER;
        report->leftStickY = PS3_STICK_CENTER;
        report->rightStickX = PS3_STICK_CENTER;
        report->rightStickY = PS3_STICK_CENTER;
        memset(report, 0, sizeof(PS3_REPORT));
        report->reportId = 1;
        convert_universal_to_type((uint8_t *)report_data, input, output_console_type);
        TICK_PS3_WITHOUT_CAPTURE;
        report_size = size = sizeof(PS3Gamepad_Data_t);
    }
    if (output_console_type != UNIVERSAL && output_console_type != WINDOWS && output_console_type != XBOX360 && output_console_type != PS3 && output_console_type != PS4 && !updateSequence) {
#else
    if (output_console_type != UNIVERSAL && output_console_type != WINDOWS && output_console_type != XBOX360 && output_console_type != PS4 && !updateSequence) {
#endif
        PS3Dpad_Data_t *gamepad = (PS3Dpad_Data_t *)report_data;
        report_size = size = sizeof(PS3_REPORT);
        memset(report_data, 0, size);
        PS3_REPORT *report = (PS3_REPORT *)report_data;
        gamepad->accelX = PS3_ACCEL_CENTER;
        gamepad->accelY = PS3_ACCEL_CENTER;
        gamepad->accelZ = PS3_ACCEL_CENTER;
        gamepad->gyro = PS3_ACCEL_CENTER;
        gamepad->leftStickX = PS3_STICK_CENTER;
        gamepad->leftStickY = PS3_STICK_CENTER;
        gamepad->rightStickX = PS3_STICK_CENTER;
        gamepad->rightStickY = PS3_STICK_CENTER;
        convert_universal_to_type((uint8_t *)report_data, input, PS3);
        TICK_PS3;
        asm volatile("" ::
                         : "memory");
        gamepad->dpad = dpad_bindings[gamepad->dpad];
#ifdef CONFIGURABLE_BLOBS
        if (SWAP_SWITCH_FACE_BUTTONS && output_console_type == SWITCH) {
            bool a = gamepad->a;
            bool b = gamepad->b;
            bool x = gamepad->x;
            bool y = gamepad->y;
            gamepad->b = a;
            gamepad->a = b;
            gamepad->x = y;
            gamepad->y = x;
        }
#elif SWAP_SWITCH_FACE_BUTTONS
        if (output_console_type == SWITCH) {
            bool a = gamepad->a;
            bool b = gamepad->b;
            bool x = gamepad->x;
            bool y = gamepad->y;
            gamepad->b = a;
            gamepad->a = b;
            gamepad->x = y;
            gamepad->y = x;
        }
#endif
    }
#if SUPPORTS_PS4
    if (output_console_type == PS4) {
        ps4_sequence_number++;
    }
#endif
#if DEVICE_TYPE_IS_NORMAL_GAMEPAD
    if (updateSequence) {
        report_sequence_number++;
        if (report_sequence_number == 0) {
            report_sequence_number = 1;
        }
    } else if (updateHIDSequence) {
        hid_sequence_number++;
        if (hid_sequence_number == 0) {
            hid_sequence_number = 1;
        }
    }
#endif
#endif
#endif
    TICK_RESET
    if (!INPUT_QUEUE && micros() - lastDebounce > 1000) {
        lastDebounce = micros();
        for (int i = 0; i < DIGITAL_COUNT; i++) {
            if (debounce[i]) {
                debounce[i]--;
            }
        }
    }
    if (output_console_type != REAL_PS3 && output_console_type != PS4 && output_console_type != PS3) {
        uint8_t cmp = memcmp(&last_report_bt, report_data, report_size);
        if (cmp == 0) {
            return 0;
        }
        memcpy(&last_report_bt, report_data, report_size);
    }
    if (size) {
        send_report_to_pc(&combined_report, size);
    }
    return size;
}
#endif
void tick(void) {
#ifdef SLAVE_TWI_PORT
    tick_slave();
#endif
#ifdef TICK_LED_STROBE
    TICK_LED_STROBE;
#endif
#ifdef TICK_LED_PERIPHERAL
    // If we are controlling peripheral leds, then we need to send the latest state when
    // the device is plugged in again
    if (slave_initted) {
        if (memcmp(lastLedStatePeripheral, ledStatePeripheral, sizeof(ledStatePeripheral)) != 0) {
            memcpy(lastLedStatePeripheral, ledStatePeripheral, sizeof(ledStatePeripheral));
            TICK_LED_PERIPHERAL;
        }
    } else {
        memset(lastLedStatePeripheral, 0, sizeof(lastLedStatePeripheral));
    }
#endif
#ifdef TICK_LED
    if (memcmp(lastLedState, ledState, sizeof(ledState)) != 0) {
        memcpy(lastLedState, ledState, sizeof(ledState));
        TICK_LED;
    }
#endif

    if (!INPUT_QUEUE && micros() - lastDebounce > 1000) {
        // No benefit to ticking bluetooth faster than this!
#ifdef BLUETOOTH_TX
        tick_bluetooth();
#endif
        lastDebounce = micros();
        for (int i = 0; i < DIGITAL_COUNT; i++) {
            if (debounce[i]) {
                debounce[i]--;
            }
        }
    }

    if (!INPUT_QUEUE && POLL_RATE && (micros() - last_poll) < (POLL_RATE * 1000)) {
        return;
    }

    bool ready = tick_usb();

    // Input queuing is enabled, tick as often as possible
    if (INPUT_QUEUE && !ready) {
        tick_inputs(NULL, NULL, consoleType);
    }

    last_poll = micros();
#if !defined(BLUETOOTH_TX)
    // Tick the controller every 5ms if this device is usb only, and usb is not ready
    if (!INPUT_QUEUE && !ready && millis() - lastSentPacket > 5) {
        lastSentPacket = millis();
        tick_inputs(NULL, NULL, consoleType);
    }
#endif
}

void device_reset(void) {
#if USB_HOST_STACK
    if (consoleType == XBOXONE) {
        if (xbox_one_state != Announce && xbox_one_state != WaitingDesc1) {
            powerMode.subcommand = 0x07;
            send_report_to_controller(get_device_address_for(XBOXONE), (uint8_t *)&powerMode, sizeof(GipPowerMode_t));
            powerMode.subcommand = 0x00;
        }
    }
#endif
    xbox_one_state = Announce;
    data_from_controller_size = 0;
    data_from_console_size = 0;
    hid_sequence_number = 1;
    report_sequence_number = 1;
    last_ghl_poke_time = 0;
}

uint8_t last_len = 0;
void receive_report_from_controller(uint8_t const *report, uint16_t len) {
    if (report[0] == GIP_INPUT_REPORT) {
        report_sequence_number = report[2] + 1;
    }
    const GipHeader_t *header = (const GipHeader_t *)report;
    if (header->command == 0x02) {
        return;
    }
    if (xbox_one_state != Auth) {
        return;
    }
    data_from_controller_size = len;
    memcpy(data_from_controller, report, len);
}


void xinput_controller_connected(uint16_t vid, uint16_t pid, uint8_t subtype) {
    if (xbox_360_state == Authenticated) return;
    xbox_360_vid = vid;
    xbox_360_pid = pid;
}

void xone_controller_connected(uint8_t dev_addr) {
    printf("Sending to controller %d\r\n", dev_addr);
    send_report_to_controller(dev_addr, (uint8_t *)&powerMode, sizeof(GipPowerMode_t));
}

void host_controller_connected() {
    // With input_usb_host, we need to run detection for a little bit after the input is detected
#ifdef INPUT_USB_HOST
    input_start = millis();
#endif
}

void ps4_controller_connected(uint8_t dev_addr, uint16_t vid, uint16_t pid) {
    if (vid == SONY_VID && (pid == PS4_DS_PID_1 || pid == PS4_DS_PID_2 || pid == PS4_DS_PID_3)) {
        handle_player_leds(1);
    }
    auth_ps4_controller_found = true;
}

void ps3_controller_connected(uint8_t dev_addr, uint16_t vid, uint16_t pid) {
    if (vid == SONY_VID && pid == SONY_DS3_PID) {
        // Enable PS3 reports
        uint8_t hid_command_enable[] = {0x42, 0x0c, 0x00, 0x00};
        transfer_with_usb_controller(dev_addr, (USB_SETUP_HOST_TO_DEVICE | USB_SETUP_RECIPIENT_INTERFACE | USB_SETUP_TYPE_CLASS), HID_REQUEST_SET_REPORT, 0x03F4, 0x00, sizeof(hid_command_enable), hid_command_enable);
        handle_player_leds(1);
    }
}

void ps4_controller_disconnected(void) {
    auth_ps4_controller_found = false;
}

void set_console_type(uint8_t new_console_type) {
    if (consoleType == new_console_type && new_console_type != UNIVERSAL) return;
    consoleType = new_console_type;
    reset_usb();
}
#if USB_HOST_STACK
USB_Device_Type_t get_usb_device_type_for(uint16_t vid, uint16_t pid) {
    USB_Device_Type_t type = {UNIVERSAL, GAMEPAD};
    switch (vid) {
        case SONY_VID:
            switch (pid) {
                case SONY_DS3_PID:
                    type.console_type = PS3;
                    break;
                case PS4_DS_PID_1:
                case PS4_DS_PID_2:
                case PS4_DS_PID_3:
                    type.console_type = PS4;
                    break;
            }
            break;
        case REDOCTANE_VID:
            switch (pid) {
                case PS3_GH_GUITAR_PID:
                    type.console_type = PS3;
                    type.sub_type = GUITAR_HERO_GUITAR;
                    break;
                case PS3_GH_DRUM_PID:
                    type.console_type = PS3;
                    type.sub_type = GUITAR_HERO_DRUMS;
                    break;
                case PS3_MPA_MUSTANG_PID:
                case PS3_MPA_SQUIRE_PID:
                case PS3_RB_GUITAR_PID:
                    type.console_type = PS3;
                    type.sub_type = ROCK_BAND_GUITAR;
                    break;
                case PS3_MPA_DRUMS_PID:
                case PS3_RB_DRUM_PID:
                    type.console_type = PS3;
                    type.sub_type = ROCK_BAND_DRUMS;
                    break;
                case PS3_DJ_TURNTABLE_PID:
                    type.console_type = PS3;
                    type.sub_type = DJ_HERO_TURNTABLE;
                    break;
                case PS3WIIU_GHLIVE_DONGLE_PID:
                    type.console_type = PS3;
                    type.sub_type = LIVE_GUITAR;
                    break;
            }
            break;

        case WII_RB_VID:
            // Polled the same as PS3, so treat them as PS3 instruments
            switch (pid) {
                case WII_MPA_MUSTANG_PID:
                case WII_MPA_SQUIRE_PID:
                case WII_RB_GUITAR_PID:
                case WII_RB_GUITAR_2_PID:
                    type.console_type = PS3;
                    type.sub_type = ROCK_BAND_GUITAR;
                    break;

                case WII_MPA_DRUMS_PID:
                case WII_RB_DRUM_PID:
                case WII_RB_DRUM_2_PID:
                    type.console_type = PS3;
                    type.sub_type = ROCK_BAND_DRUMS;
                    break;
            }
            break;

        case XBOX_ONE_RB_VID:
            if (pid == XBOX_ONE_RB_GUITAR_PID) {
                type.console_type = XBOXONE;
                type.sub_type = ROCK_BAND_GUITAR;
            } else if (pid == XBOX_ONE_RB_DRUM_PID) {
                type.console_type = XBOXONE;
                type.sub_type = ROCK_BAND_DRUMS;
            }
            break;

        case XBOX_ONE_GHLIVE_DONGLE_VID:
            if (pid == XBOX_ONE_GHLIVE_DONGLE_PID) {
                type.console_type = XBOXONE;
                type.sub_type = LIVE_GUITAR;
            }
            break;
    }
    return type;
}
#endif