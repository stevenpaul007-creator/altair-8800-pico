#line 1 "/home/seven/codebuddy-work/altair-8800-pico/panel_config.h"
// =============================================================================
// panel_config.h
// -----------------------------------------------------------------------------
// Physical front-panel wiring for the RP2040 port of Altair101a, per the user's
// PCA9555 plan. This is the ONLY file you edit to change the panel wiring.
//
// Chips: 0x20, 0x21, 0x22, 0x23 (indices 0..3 in PANEL_CHIP_ADDR).
//
// --- LED polarity: all LEDs are ACTIVE LOW (a LOW output lights the LED). ---
// --- Switch polarity: all switches are ACTIVE LOW (a LOW input = ON).      ---
//
// Bit-order note (the user said "reversed"):
//   * On 0x20 io1, the 8 data/address switches are mapped with io1_k -> bit (7-k).
//     (io1_0 = bit 7 ... io1_7 = bit 0.)  These are address bits A0..A7 AND the
//     deposited data byte (low 8 bits of fpAddressToggleWord).
//   * On 0x22 io1, the 8 address switches are mapped with io1_k -> bit (15-k).
//     (io1_0 = bit 15 ... io1_7 = bit 8.)  These are address bits A8..A15.
//   => EXAMINE uses the full 16-bit fpAddressToggleWord as the address;
//      DEPOSIT writes its low 8 bits as the data byte (classic Altair layout).
//   * Switches reflect their physical position (bit = switch state), not toggle.
// =============================================================================

#ifndef PANEL_CONFIG_H
#define PANEL_CONFIG_H

static const uint8_t PANEL_CHIP_ADDR[4] = { 0x20, 0x21, 0x22, 0x23 };

// -----------------------------------------------------------------------------
// LED mapping (logical LED -> chip/port/bit). Outputs, active LOW.
// -----------------------------------------------------------------------------
static const PanelPin ledMap[LED_COUNT] = {
  // Address LEDs A0..A7  -> chip 0x21 (idx1), io0
  {1,0,0}, {1,0,1}, {1,0,2}, {1,0,3}, {1,0,4}, {1,0,5}, {1,0,6}, {1,0,7},
  // Address LEDs A8..A15 -> chip 0x23 (idx3), io0
  {3,0,0}, {3,0,1}, {3,0,2}, {3,0,3}, {3,0,4}, {3,0,5}, {3,0,6}, {3,0,7},
  // Data LEDs D0..D7     -> chip 0x20 (idx0), io0
  {0,0,0}, {0,0,1}, {0,0,2}, {0,0,3}, {0,0,4}, {0,0,5}, {0,0,6}, {0,0,7},
  // Status LEDs -> chip 0x22 (idx2), io0, order: INT,WO,STACK,HLTA,OUT,M1,INP,MEMR
  {2,0,0}, {2,0,1}, {2,0,2}, {2,0,3}, {2,0,4}, {2,0,5}, {2,0,6}, {2,0,7},
  // WAIT, HLDA, INTE, PROT -> chip 0x23 (idx3), io1 (INTE=0, PROT=1, WAIT=2, HLDA=3)
  {3,1,2}, {3,1,3}, {3,1,0}, {3,1,1},
};

// -----------------------------------------------------------------------------
// Button / switch mapping (logical button -> chip/port/bit). Inputs, active LOW.
// For switches, twBit = fpAddressToggleWord bit index. For command buttons, cmd =
// the ASCII key sent to the emulator (0 = none / handled specially).
// -----------------------------------------------------------------------------
static const PanelPin btnMap[BTN_COUNT] = {
  // --- chip 0x21 (idx1), io1: command buttons ---
  {1,1,0,0xFF,'x'},   // EXAMINE
  {1,1,1,0xFF,'X'},   // EXAMINE NEXT
  {1,1,2,0xFF,'p'},   // DEPOSIT
  {1,1,3,0xFF,'P'},   // DEPOSIT NEXT
  {1,1,4,0xFF,'R'},   // RESET
  {1,1,5,0xFF,'C'},   // CLR
  {1,1,6,0xFF,0},     // PROTECT   (toggles status_prot)
  {1,1,7,0xFF,0},     // UNPROTECT (clears status_prot)
  // --- chip 0x23 (idx3), io1_4..io1_7: command buttons ---
  {3,1,4,0xFF,'s'},   // STOP      (STOP/SINGLE-STEP depends on run state)
  {3,1,5,0xFF,'r'},   // RUN
  {3,1,6,0xFF,'s'},   // SINGLE STEP
  {3,1,7,0xFF,0},     // AUX (no function yet)
  // --- chip 0x20 (idx0), io1: data/address switches A0..A7 (reversed: io1_k -> bit 7-k) ---
  {0,1,0,7,0}, {0,1,1,6,0}, {0,1,2,5,0}, {0,1,3,4,0},
  {0,1,4,3,0}, {0,1,5,2,0}, {0,1,6,1,0}, {0,1,7,0,0},
  // --- chip 0x22 (idx2), io1: address switches A8..A15 (reversed: io1_k -> bit 15-k) ---
  {2,1,0,15,0}, {2,1,1,14,0}, {2,1,2,13,0}, {2,1,3,12,0},
  {2,1,4,11,0}, {2,1,5,10,0}, {2,1,6,9,0},  {2,1,7,8,0},
};

#endif // PANEL_CONFIG_H
