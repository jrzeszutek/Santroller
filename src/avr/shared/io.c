#include "io.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <math.h>
#include <stdio.h>
#include <util/delay.h>
#include <util/twi.h>
#include <string.h>
#ifndef TWI_BUFFER_LENGTH
#include "string.h"
#  define TWI_BUFFER_LENGTH 32
#endif

#define INTERRUPT_PS2_ACK_EICRA _BV(ISC00) | _BV(ISC01)
#define INTERRUPT_PS2_ACK_VECT INTERRUPT_PS2_ACK##_vect


#define TWI_READY 0
#define TWI_MRX 1
#define TWI_MTX 2
#define TWI_SRX 3
#define TWI_STX 4
volatile bool spi_acknowledged = false;
void spi_begin() {
#ifdef SPI_CLOCK
    pinMode(PIN_SPI_MOSI, OUTPUT);
    pinMode(PIN_SPI_MISO, INPUT_PULLUP);
    pinMode(PIN_SPI_SCK, OUTPUT);
    digitalWrite(PIN_SPI_SS, 1);
    pinMode(PIN_SPI_SS, OUTPUT);
    uint8_t clockDiv;
    if (SPI_CLOCK >= F_CPU / 2) {
        clockDiv = 0;
    } else if (SPI_CLOCK >= F_CPU / 4) {
        clockDiv = 1;
    } else if (SPI_CLOCK >= F_CPU / 8) {
        clockDiv = 2;
    } else if (SPI_CLOCK >= F_CPU / 16) {
        clockDiv = 3;
    } else if (SPI_CLOCK >= F_CPU / 32) {
        clockDiv = 4;
    } else if (SPI_CLOCK >= F_CPU / 64) {
        clockDiv = 5;
    } else {
        clockDiv = 7;
    }

    // Invert the SPI2X bit
    clockDiv ^= 0x1;

    uint8_t config = 0;
#if SPI_CPHA == 1
    config |= _BV(CPHA);
#endif
#if SPI_CPOL == 1
    config |= _BV(CPOL);
#endif
#if SPI_MSBFIRST == 0
    config |= _BV(DORD);
#endif
    if (cpha) {
        config |= _BV(CPHA);
    }
    SPCR = _BV(SPE) | _BV(MSTR) | config | ((clockDiv >> 1) & 0x03);
    SPSR = clockDiv & 0x01;
#endif
}
uint8_t spi_transfer(SPI_BLOCK block, uint8_t data) {
    SPDR = data;
    asm volatile("nop");
    while (!(SPSR & _BV(SPIF)))
        ;
    return SPDR;
}
void spi_high(SPI_BLOCK block) {}
void twi_init() {
#ifdef TWI_FREQ
    twi_state = TWI_READY;
    twi_sendStop = true;  // default value
    twi_inRepStart = false;

// // activate internal pullups for twi.
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__) || \
    defined(__AVR_ATmega328P__)
    // activate internal pull-ups for twi
    // as per note from atmega8 manual pg167
    sbi(PORTC, 4);
    sbi(PORTC, 5);
#else
    // activate internal pull-ups for twi
    // as per note from atmega128 manual pg204
    sbi(PORTD, 0);
    sbi(PORTD, 1);
#endif

    // initialize twi prescaler and bit rate
    cbi(TWSR, TWPS0);
    cbi(TWSR, TWPS1);
// note: TWBR should be 10 or higher for master mode
#if ((F_CPU / TWI_FREQ) - 16) / 2 > 10
    TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;
#else
    TWBR = 10;
#endif
    // enable twi module, acks, and twi interrupt
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
#endif
}

#ifdef PS2_ACK
ISR(INTERRUPT_PS2_ACK_VECT) {
    spi_acknowledged = true;
}
// TODO: we need to define these
void init_ack() {
    cli();
    EICRA |= INTERRUPT_PS2_ACK_EICRA;
    EIMSK |= _BV(INTERRUPT_PS2_ACK);
    sei();
}
#endif

static volatile uint8_t twi_state;
static volatile uint8_t twi_slarw;
static volatile uint8_t twi_sendStop; // should the transaction end with a stop
static volatile uint8_t twi_inRepStart; // in the middle of a repeated start

static uint8_t twi_masterBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t twi_masterBufferIndex;
static volatile uint8_t twi_masterBufferLength;

static volatile uint8_t twi_error;

// === MODIFIED ===
static uint16_t TIMEOUT = 32767;

bool twi_readFromPointerSlow(TWI_BLOCK block, uint8_t address, uint8_t pointer, uint8_t length,
                             uint8_t *data) {
  if (!twi_writeTo(block, address, &pointer, 1, true, true)) return false;
  _delay_us(180);
  return twi_readFrom(block, address, data, length, true);
}
bool twi_readFrom(TWI_BLOCK block, uint8_t address, uint8_t *data, uint8_t length,
                  uint8_t sendStop) {

  // ensure data will fit into buffer
  if (TWI_BUFFER_LENGTH < length) return 0;

  uint16_t timeoutCounter;

  // === MODIFIED ===
  // wait until twi is ready, become master receiver
  if (TIMEOUT == 0) {
    while (TWI_READY != twi_state) continue;
  } else {
    timeoutCounter = TIMEOUT;
    while (TWI_READY != twi_state) {
      //_delay_ms( TIMEOUT_TICK );
      timeoutCounter--;
      if (timeoutCounter == 0) return 0;
    }
  }
  /*
  // wait until twi is ready, become master receiver
  while(TWI_READY != twi_state){
    continue;
  }
  */

  twi_state = TWI_MRX;
  twi_sendStop = sendStop;
  // reset error state (0xFF.. no error occured)
  twi_error = 0xFF;

  // initialize buffer iteration vars
  twi_masterBufferIndex = 0;
  twi_masterBufferLength = length - 1; // This is not intuitive, read on...
  // On receive, the previously configured ACK/NACK setting is transmitted in
  // response to the received byte before the interrupt is signalled.
  // Therefor we must actually set NACK when the _next_ to last byte is
  // received, causing that NACK to be sent in response to receiving the last
  // expected byte of data.

  // build sla+w, slave device address + w bit
  twi_slarw = TW_READ;
  twi_slarw |= address << 1;

  if (twi_inRepStart) {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address
    // byte. We need to remove ourselves from the repeated start state before we
    // enable interrupts, since the ISR is ASYNC, and we could get confused if
    // we hit the ISR before cleaning up. Also, don't enable the START
    // interrupt. There may be one pending from the repeated start that we sent
    // ourselves, and that would really confuse things.
    twi_inRepStart = false; // remember, we're dealing with an ASYNC ISR

    // === MODIFIED ===
    if (TIMEOUT == 0) {
      do { TWDR = twi_slarw; } while (TWCR & _BV(TWWC));
    } else {
      timeoutCounter = TIMEOUT;
      TWDR = twi_slarw;
      while (TWCR & _BV(TWWC)) {
        //_delay_ms( TIMEOUT_TICK );
        timeoutCounter--;
        if (timeoutCounter == 0) return false;
        TWDR = twi_slarw;
      }
    }
    /*
    do {
      TWDR = twi_slarw;
    } while(TWCR & _BV(TWWC));
    */
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) |
           _BV(TWIE); // enable INTs, but not START
  } else {
    // send start condition
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTA);
  }

  // === MODIFIED ===
  // wait for read operation to complete
  if (TIMEOUT == 0) {
    while (TWI_MRX == twi_state) continue;
  } else {
    timeoutCounter = TIMEOUT;
    while (TWI_MRX == twi_state) {
      //_delay_ms( TIMEOUT_TICK );
      timeoutCounter--;
      if (timeoutCounter == 0) return false;
    }
  }
  /*
  // wait for read operation to complete
  while(TWI_MRX == twi_state){
    continue;
  }
  */

  if (twi_masterBufferIndex < length) length = twi_masterBufferIndex;
  // copy twi buffer to data
  memcpy(data, twi_masterBuffer, length);

  return length;
}

bool twi_writeTo(TWI_BLOCK block,uint8_t address, uint8_t *data, uint8_t length, uint8_t wait,
                 uint8_t sendStop) {

  // ensure data will fit into buffer
  if (TWI_BUFFER_LENGTH < length) return false;

  // === MODIFIED ===
  uint16_t timeoutCounter;
  // wait until twi is ready, become master transmitter
  if (TIMEOUT == 0) {
    while (TWI_READY != twi_state) continue;
  } else {
    timeoutCounter = TIMEOUT;
    while (TWI_READY != twi_state) {
      //_delay_ms( TIMEOUT_TICK );
      timeoutCounter--;
      if (timeoutCounter == 0) return false;
    }
  }
  /*
  // wait until twi is ready, become master transmitter
  while( TWI_READY != twi_state ) {
    continue;
  }
  */

  twi_state = TWI_MTX;
  twi_sendStop = sendStop;
  // reset error state (0xFF.. no error occured)
  twi_error = 0xFF;

  // initialize buffer iteration vars
  twi_masterBufferIndex = 0;
  twi_masterBufferLength = length;

  // copy data to twi buffer
  memcpy(twi_masterBuffer, data, length);
  // build sla+w, slave device address + w bit
  twi_slarw = TW_WRITE;
  twi_slarw |= address << 1;

  // if we're in a repeated start, then we've already sent the START
  // in the ISR. Don't do it again.
  if (twi_inRepStart) {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address
    // byte. We need to remove ourselves from the repeated start state before we
    // enable interrupts, since the ISR is ASYNC, and we could get confused if
    // we hit the ISR before cleaning up. Also, don't enable the START
    // interrupt. There may be one pending from the repeated start that we sent
    // outselves, and that would really confuse things.
    twi_inRepStart = false; // remember, we're dealing with an ASYNC ISR

    // === MODIFIED ===
    if (TIMEOUT == 0) {
      do { TWDR = twi_slarw; } while (TWCR & _BV(TWWC));
    } else {
      timeoutCounter = TIMEOUT;
      TWDR = twi_slarw;
      while (TWCR & _BV(TWWC)) {
        //_delay_ms( TIMEOUT_TICK );
        timeoutCounter--;
        if (timeoutCounter == 0) return false;
        TWDR = twi_slarw;
      }
    }
    /*
    do {
      TWDR = twi_slarw;
    } while(TWCR & _BV(TWWC));
    */

    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) |
           _BV(TWIE); // enable INTs, but not START
  } else {
    // send start condition
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE) |
           _BV(TWSTA); // enable INTs
  }

  // === MODIFIED ===
  // wait for write operation to complete
  if (TIMEOUT == 0) {
    while (wait && (TWI_MTX == twi_state)) continue;
  } else {
    timeoutCounter = TIMEOUT;
    while (wait && (TWI_MTX == twi_state)) {
      //_delay_ms( TIMEOUT_TICK );
      timeoutCounter--;
      if (timeoutCounter == 0) return false;
    }
  }
  /*
  // wait for write operation to complete
  while(wait && (TWI_MTX == twi_state)){
    continue;
  }*/

  return twi_error == 0xFF;
}

void twi_reply(uint8_t ack) {
  // transmit master read ready signal, with or without ack
  if (ack) {
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
  } else {
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
  }
}

void twi_releaseBus(void)
{
  // release bus
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT);

  // update twi state
  twi_state = TWI_READY;
}

bool twi_stop(void) {
  // send stop condition
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTO);

  // === MODIFIED ===
  // wait for stop condition to be exectued on bus
  // TWINT is not set after a stop condition!
  if (TIMEOUT == 0) {
    while (TWCR & _BV(TWSTO)) continue;
  } else {
    uint16_t timeoutCounter = TIMEOUT;
    while (TWCR & _BV(TWSTO)) {
      //_delay_ms( TIMEOUT_TICK );
      timeoutCounter--;
      if (timeoutCounter == 0) return false;
    }
  }
  /*
  while(TWCR & _BV(TWSTO)){
    continue;
  }
  */

  // update twi state
  twi_state = TWI_READY;
  return true;
}


ISR(TWI_vect) {
  switch (TW_STATUS) {
  // All Master
  case TW_START:     // sent start condition
  case TW_REP_START: // sent repeated start condition
    // copy device address and r/w bit to output register and ack
    TWDR = twi_slarw;
    twi_reply(1);
    break;

  // Master Transmitter
  case TW_MT_SLA_ACK:  // slave receiver acked address
  case TW_MT_DATA_ACK: // slave receiver acked data
    // if there is data to send, send it, otherwise stop
    if (twi_masterBufferIndex < twi_masterBufferLength) {
      // copy data to output register and ack
      TWDR = twi_masterBuffer[twi_masterBufferIndex++];
      twi_reply(1);
    } else {
      if (twi_sendStop)
        twi_stop();
      else {
        twi_inRepStart = true; // we're gonna send the START
        // don't enable the interrupt. We'll generate the start, but we
        // avoid handling the interrupt until we're in the next transaction,
        // at the point where we would normally issue the start.
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        twi_state = TWI_READY;
      }
    }
    break;
  case TW_MT_SLA_NACK: // address sent, nack received
    twi_error = TW_MT_SLA_NACK;
    twi_stop();
    break;
  case TW_MT_DATA_NACK: // data sent, nack received
    twi_error = TW_MT_DATA_NACK;
    twi_stop();
    break;
  case TW_MT_ARB_LOST: // lost bus arbitration
    twi_error = TW_MT_ARB_LOST;
    twi_releaseBus();
    break;

  // Master Receiver
  case TW_MR_DATA_ACK: // data received, ack sent
    // put byte into buffer
    twi_masterBuffer[twi_masterBufferIndex++] = TWDR;
  case TW_MR_SLA_ACK: // address sent, ack received
    // ack if more bytes are expected, otherwise nack
    if (twi_masterBufferIndex < twi_masterBufferLength) {
      twi_reply(1);
    } else {
      twi_reply(0);
    }
    break;
  case TW_MR_DATA_NACK: // data received, nack sent
    // put final byte into buffer
    twi_masterBuffer[twi_masterBufferIndex++] = TWDR;
    if (twi_sendStop)
      twi_stop();
    else {
      twi_inRepStart = true; // we're gonna send the START
      // don't enable the interrupt. We'll generate the start, but we
      // avoid handling the interrupt until we're in the next transaction,
      // at the point where we would normally issue the start.
      TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
      twi_state = TWI_READY;
    }
    break;
  case TW_MR_SLA_NACK: // address sent, nack received
    twi_stop();
    break;
  // TW_MR_ARB_LOST handled by TW_MT_ARB_LOST case

  // All
  case TW_NO_INFO: // no state information
    break;
  case TW_BUS_ERROR: // bus error, illegal stop/start
    twi_error = TW_BUS_ERROR;
    twi_stop();
    break;
  }
}