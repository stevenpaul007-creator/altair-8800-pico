#line 1 "/home/seven/codebuddy-work/altair-8800-pico/Panel9555.h"
// =============================================================================
// Panel9555.h
// -----------------------------------------------------------------------------
// Front-panel driver for the RP2040 port of Altair101a.
//
// The emulator core keeps the canonical LED state in fpAddressWord / fpDataByte /
// fpStatusByte / status_wait / status_hlda / status_inte. This module only reads
// those variables and pushes changes to the hardware.
//
// Hardware plan (user-supplied, see panel_config.h for the exact pin table):
//   0x20  io0 = DATA LEDs D0..D7 (active LOW)        io1 = DATA/ADDR switches A0..A7
//   0x21  io0 = ADDR LEDs A0..A7 (active LOW)        io1 = command buttons
//   0x22  io0 = STATUS LEDs (active LOW)             io1 = ADDR switches A8..A15
//   0x23  io0 = ADDR LEDs A8..A15 (active LOW)       io1 = STATUS LEDs INTE/PROT/WAIT/HLDA + command buttons
//
// All LEDs are active LOW (a LOW output lights the LED).
// All buttons/switches are active LOW (a LOW input means pressed / ON).
// =============================================================================

#ifndef PANEL_9555_H
#define PANEL_9555_H

#include <Arduino.h>
#include <Wire.h>

// ---- I2C bus used for the panel -------------------------------------------
#define PANEL_WIRE  Wire1
#define PANEL_SDA_PIN 6
#define PANEL_SCL_PIN 7

// ---- PCA9555 registers -----------------------------------------------------
#define PCA9555_IN0  0x00
#define PCA9555_IN1  0x01
#define PCA9555_OUT0 0x02
#define PCA9555_OUT1 0x03
#define PCA9555_POL0 0x04
#define PCA9555_POL1 0x05
#define PCA9555_CFG0 0x06
#define PCA9555_CFG1 0x07

// ---- Logical LED indices ----------------------------------------------------
enum PanelLed {
  LED_A0 = 0, LED_A1, LED_A2, LED_A3, LED_A4, LED_A5, LED_A6, LED_A7,
  LED_A8, LED_A9, LED_A10, LED_A11, LED_A12, LED_A13, LED_A14, LED_A15, // 0..15
  LED_D0, LED_D1, LED_D2, LED_D3, LED_D4, LED_D5, LED_D6, LED_D7,       // 16..23
  LED_ST_MEMR, LED_ST_INP, LED_ST_M1, LED_ST_OUT, LED_ST_HLTA,
  LED_ST_STACK, LED_ST_WO, LED_ST_INT,                                   // 24..31
  LED_WAIT, LED_HLDA, LED_INTE, LED_PROT,                                // 32..35
  LED_COUNT
};

// ---- Logical button indices -------------------------------------------------
enum PanelBtn {
  // Command buttons on chip 0x21 (io1_0..io1_7)
  BTN_EXAMINE = 0, BTN_EXAMINE_NEXT, BTN_DEPOSIT, BTN_DEPOSIT_NEXT,
  BTN_RESET, BTN_CLR, BTN_PROTECT, BTN_UNPROTECT,                       // 0..7
  // Command buttons on chip 0x23 (io1_4..io1_7)
  BTN_STOP, BTN_RUN, BTN_SINGLE_STEP, BTN_AUX,                          // 8..11
  // Toggle switches (data/address). twBit is the fpAddressToggleWord bit index.
  BTN_SW0, BTN_SW1, BTN_SW2, BTN_SW3, BTN_SW4, BTN_SW5, BTN_SW6, BTN_SW7,    // 12..19
  BTN_SW8, BTN_SW9, BTN_SW10, BTN_SW11, BTN_SW12, BTN_SW13, BTN_SW14, BTN_SW15, // 20..27
  BTN_COUNT
};

// A physical pin: which chip (0..3), which port (0 or 1), which bit (0..7).
// twBit: 0..15 = this button is a toggle switch mapped to that fpAddressToggleWord
//         bit; 0xFF means it is a command button (use 'cmd' instead).
// cmd:   ASCII key to send to the emulator when the button is pressed (0 = none).
struct PanelPin { uint8_t chip; uint8_t port; uint8_t bit; uint8_t twBit; uint8_t cmd; };

#include "panel_config.h"

// ---- Public API ------------------------------------------------------------
void     panelBegin(void);
void     panelLedsUpdate(void);
void     panelService(void);          // Core0: refresh LEDs + drain panel events
void     panelScanButtons(void);      // Core1: scan buttons/switches, fill key queue
extern volatile bool panelReady;      // set by panelBegin once I2C is up
bool     consoleAvailable(void);
int      consoleRead(void);
byte     panelReadSenseByte(bool highlow);

#endif // PANEL_9555_H
