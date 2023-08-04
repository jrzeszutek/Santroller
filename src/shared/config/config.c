#include "config.h"

#include "defines.h"
#include "progmem.h"
uint8_t consoleType = CONSOLE_TYPE;
#if SUPPORTS_PICO
const uint8_t __attribute__((section(".santroller_config"))) config[CONFIGURATION_LEN] = CONFIGURATION;
#ifdef CONFIGURABLE_BLOBS
const uint16_t __attribute__((section(".santroller_config_blobs"))) config_blobs[CONFIGURABLE_BLOBS_LEN] = CONFIGURABLE_BLOBS;
const char __attribute__((section(".santroller_config_variant"))) variant[] = VARIANT;
#endif
#else
const uint8_t PROGMEM config[CONFIGURATION_LEN] = CONFIGURATION;
#endif
#if DEVICE_TYPE_IS_GUITAR
const uint8_t announce[] = {0x02, 0x20, 0x01, 0x1c, 0x7e, 0xed, 0x81, TEMP, TEMP, TEMP, 0x00, 0x00, 0x38, 0x07, 0x61, 0x41, 0x01, 0x00, 0x00, 0x00, 0xe5, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00};
const uint8_t xb1_descriptor[] = {0x04, 0xf0, 0x01, 0x3a, 0xe5, 0x01, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe5, 0x00, 0xa6, 0x00, 0x16, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x23, 0x00, 0x29, 0x00, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x06, 0x07, 0x05, 0x01, 0x04, 0x05, 0x06, 0x0a, 0x02, 0x04, 0xa0, 0x01, 0xba, 0x00, 0x3a, 0x20, 0x00, 0x4d, 0x61, 0x64, 0x43, 0x61, 0x74, 0x7a, 0x2e, 0x58, 0x62, 0x6f, 0x78, 0x2e, 0x47, 0x75, 0x69, 0x74, 0x61, 0x72, 0x2e, 0x53, 0x74, 0x72, 0x61, 0x74, 0x6f, 0x63, 0x61, 0x73, 0x74, 0x65, 0x72, 0x27, 0x00, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x73, 0x2e, 0x58, 0x62, 0x6f, 0x78, 0x2e, 0x49, 0x6e, 0x70, 0x75, 0x74, 0x2e, 0x4e, 0x61, 0x76, 0x04, 0xa0, 0x01, 0xba, 0x00, 0x74, 0x69, 0x67, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x43, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x6c, 0x65, 0x72, 0x03, 0x38, 0xe4, 0x2a, 0x0d, 0x7d, 0x7f, 0x33, 0x49, 0x86, 0x93, 0x30, 0xfc, 0x55, 0x01, 0x8e, 0x77, 0xe7, 0x1f, 0xf3, 0xb8, 0x86, 0x73, 0xe9, 0x40, 0xa9, 0xf8, 0x2f, 0x21, 0x26, 0x3a, 0xcf, 0xb7, 0x56, 0xff, 0x76, 0x97, 0xfd, 0x9b, 0x81, 0x45, 0x04, 0xb0, 0x01, 0x37, 0xae, 0x01, 0xad, 0x45, 0xb6, 0x45, 0xbb, 0xa5, 0x26, 0xd6, 0x02, 0x17, 0x00, 0x20, 0x0e, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x21, 0x05, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t xb1_descriptor_end[] = {0x04, 0xa0, 0x01, 0x00, 0xe5, 0x01};
#elif DEVICE_TYPE_IS_DRUM
const uint8_t announce[] = {0x02, 0x20, 0x01, 0x1c, 0x7e, 0xed, 0x81, TEMP, TEMP, TEMP, 0x00, 0x00, 0x38, 0x07, 0x62, 0x42, 0x01, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00};
const uint8_t xb1_descriptor[] = {0x04, 0xf0, 0x01, 0x3a, 0xc5, 0x01, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc5, 0x00, 0x9d, 0x00, 0x16, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x23, 0x00, 0x29, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x06, 0x07, 0x05, 0x01, 0x04, 0x05, 0x06, 0x0a, 0x02, 0x04, 0xa0, 0x01, 0xba, 0x00, 0x3a, 0x17, 0x00, 0x4d, 0x61, 0x64, 0x43, 0x61, 0x74, 0x7a, 0x2e, 0x58, 0x62, 0x6f, 0x78, 0x2e, 0x44, 0x72, 0x75, 0x6d, 0x73, 0x2e, 0x47, 0x6c, 0x61, 0x6d, 0x27, 0x00, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x73, 0x2e, 0x58, 0x62, 0x6f, 0x78, 0x2e, 0x49, 0x6e, 0x70, 0x75, 0x74, 0x2e, 0x4e, 0x61, 0x76, 0x69, 0x67, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x43, 0x6f, 0x04, 0xa0, 0x01, 0xba, 0x00, 0x74, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x6c, 0x65, 0x72, 0x03, 0x93, 0x28, 0x18, 0x06, 0xe0, 0xcc, 0x85, 0x4b, 0x92, 0x71, 0x0a, 0x10, 0xdb, 0xab, 0x7e, 0x07, 0xe7, 0x1f, 0xf3, 0xb8, 0x86, 0x73, 0xe9, 0x40, 0xa9, 0xf8, 0x2f, 0x21, 0x26, 0x3a, 0xcf, 0xb7, 0x56, 0xff, 0x76, 0x97, 0xfd, 0x9b, 0x81, 0x45, 0xad, 0x45, 0xb6, 0x45, 0xbb, 0xa5, 0x26, 0xd6, 0x01, 0x04, 0xb0, 0x01, 0x17, 0xae, 0x01, 0x17, 0x00, 0x20, 0x0a, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t xb1_descriptor_end[] = {0x04, 0xa0, 0x01, 0x00, 0xc5, 0x01};
#elif DEVICE_TYPE_IS_LIVE_GUITAR
const uint8_t announce[] = {0x02, 0x20, 0x01, 0x1C, 0x7B, 0xED, 0x63, TEMP, TEMP, TEMP, 0x00, 0x00, 0x30, 0x14, 0x9B, 0x07, 0x01, 0x00, 0x00, 0x00, 0x10, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00};
const uint8_t xb1_descriptor[] = {0x04, 0xF0, 0x01, 0x3A, 0xA5, 0x02, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x01, 0xCF, 0x00, 0x16, 0x00, 0x1F, 0x00, 0x20, 0x00, 0x27, 0x00, 0x2D, 0x00, 0x8E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x06, 0x07, 0x05, 0x01, 0x04,
                                  0x04, 0xA0, 0x01, 0xBA, 0x00, 0x3A, 0x05, 0x06, 0x0A, 0x03, 0x19, 0x00, 0x41, 0x63, 0x74, 0x69, 0x76, 0x69, 0x73, 0x69, 0x6F, 0x6E, 0x2E, 0x58, 0x62, 0x6F, 0x78, 0x2E, 0x49, 0x6E, 0x70, 0x75, 0x74, 0x2E, 0x47, 0x48, 0x37, 0x1A, 0x00, 0x57, 0x69, 0x6E, 0x64, 0x6F, 0x77, 0x73, 0x2E, 0x58, 0x62, 0x6F, 0x78, 0x2E, 0x49, 0x6E, 0x70, 0x75, 0x74, 0x2E, 0x47, 0x61, 0x6D, 0x65, 0x70, 0x61,
                                  0x04, 0xA0, 0x01, 0xBA, 0x00, 0x74, 0x64, 0x27, 0x00, 0x57, 0x69, 0x6E, 0x64, 0x6F, 0x77, 0x73, 0x2E, 0x58, 0x62, 0x6F, 0x78, 0x2E, 0x49, 0x6E, 0x70, 0x75, 0x74, 0x2E, 0x4E, 0x61, 0x76, 0x69, 0x67, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x43, 0x6F, 0x6E, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x65, 0x72, 0x04, 0xD9, 0xFD, 0x12, 0xFD, 0x73, 0x8E, 0xC7, 0x47, 0xA2, 0x31, 0x96, 0x26, 0x8C, 0x38, 0x00,
                                  0x04, 0xA0, 0x01, 0x3A, 0xAE, 0x01, 0x9A, 0x2C, 0x40, 0x2E, 0x08, 0xDF, 0x07, 0xE1, 0x45, 0xA5, 0xAB, 0xA3, 0x12, 0x7A, 0xF1, 0x97, 0xB5, 0xE7, 0x1F, 0xF3, 0xB8, 0x86, 0x73, 0xE9, 0x40, 0xA9, 0xF8, 0x2F, 0x21, 0x26, 0x3A, 0xCF, 0xB7, 0x56, 0xFF, 0x76, 0x97, 0xFD, 0x9B, 0x81, 0x45, 0xAD, 0x45, 0xB6, 0x45, 0xBB, 0xA5, 0x26, 0xD6, 0x03, 0x17, 0x00, 0x20, 0x0E, 0x00, 0x01, 0x00, 0x10,
                                  0x04, 0xA0, 0x01, 0x3A, 0xE8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x21, 0x20, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x22, 0x08, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x04, 0xB0, 0x01, 0x03, 0xA2, 0x02, 0x00, 0x00, 0x00};

const uint8_t xb1_descriptor_end[] = {0x04, 0xA0, 0x01, 0x00, 0xA5, 0x02};
#else  // Gamepad
const uint8_t announce[] = {
    0x02, 0x20, 0x02, 0x1c, 0x7e, 0xed, 0x8d, TEMP,
    TEMP, TEMP, 0x00, 0x00, 0x5e, 0x04, 0xea, 0x02,
    0x05, 0x00, 0x11, 0x00, 0x82, 0x0c, 0x00, 0x00,
    0x04, 0x05, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00};

const uint8_t xb1_descriptor[] = {
    0x04, 0xf0, 0x03, 0x3a, 0xdc, 0x01, 0x10, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x9d, 0x00,
    0x16, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x26, 0x00,
    0x2f, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x11,
    0x00, 0x00, 0x09, 0x01, 0x02, 0x03, 0x04, 0x06,
    0x07, 0x0c, 0x0d, 0x1e, 0x08, 0x01, 0x04, 0x05,
    0x04, 0xa0, 0x03, 0xba, 0x00, 0x3a, 0x06, 0x0a,
    0x0c, 0x0d, 0x1e, 0x01, 0x1a, 0x00, 0x57, 0x69,
    0x6e, 0x64, 0x6f, 0x77, 0x73, 0x2e, 0x58, 0x62,
    0x6f, 0x78, 0x2e, 0x49, 0x6e, 0x70, 0x75, 0x74,
    0x2e, 0x47, 0x61, 0x6d, 0x65, 0x70, 0x61, 0x64,
    0x05, 0x56, 0xff, 0x76, 0x97, 0xfd, 0x9b, 0x81,
    0x45, 0xad, 0x45, 0xb6, 0x45, 0xbb, 0xa5, 0x26,
    0xd6, 0x2c, 0x40, 0x2e, 0x08, 0xdf, 0x07, 0xe1,
    0x04, 0xa0, 0x03, 0xba, 0x00, 0x74, 0x45, 0xa5,
    0xab, 0xa3, 0x12, 0x7a, 0xf1, 0x97, 0xb5, 0xe7,
    0x1f, 0xf3, 0xb8, 0x86, 0x73, 0xe9, 0x40, 0xa9,
    0xf8, 0x2f, 0x21, 0x26, 0x3a, 0xcf, 0xb7, 0x6b,
    0xe5, 0xf2, 0x87, 0xbb, 0xc3, 0xb1, 0x49, 0x82,
    0x65, 0xff, 0xff, 0xf3, 0x77, 0x99, 0xee, 0x1e,
    0x9b, 0xad, 0x34, 0xad, 0x36, 0xb5, 0x4f, 0x8a,
    0xc7, 0x17, 0x23, 0x4c, 0x9f, 0x54, 0x6f, 0x02,
    0x04, 0xb0, 0x03, 0x2e, 0xae, 0x01, 0x17, 0x00,
    0x20, 0x2c, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x09,
    0x3c, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

const uint8_t xb1_descriptor_end[] = {
    0x04, 0xa0, 0x03, 0x00, 0xdc, 0x01};
#endif