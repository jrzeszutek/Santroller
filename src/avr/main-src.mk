SRC += ${PROJECT_ROOT}/lib/avr-nrf24l01/src/nrf24l01.c ${PROJECT_ROOT}/src/shared/output/reports.c ${PROJECT_ROOT}/src/shared/util/util.c ${PROJECT_ROOT}/src/shared/output/serial_handler.c $(shell find ${PROJECT_ROOT}/src/shared/output/reports/ -type f -name '*.c*') $(shell find ${PROJECT_ROOT}/src/avr/lib/spi -type f -name '*.c*') $(shell find ${PROJECT_ROOT}/src/avr/lib/i2c -type f -name '*.c*') $(shell find ${PROJECT_ROOT}/lib/mpu6050 -type f -name '*.c*') ${PROJECT_ROOT}/src/shared/leds/leds.c $(shell find ${PROJECT_ROOT}/src/shared/input -type f -name '*.c*') $(shell find ${PROJECT_ROOT}/src/shared/config -type f -name '*.c*') 