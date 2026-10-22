#line 1 "/home/seven/codebuddy-work/altair-8800-pico/Panel9555.cpp"
// =============================================================================
// Panel9555.cpp
// -----------------------------------------------------------------------------
// Front-panel driver implementation. See Panel9555.h for the architecture.
//
// Dual-core note (RP2040):
//   Core0 (machine core)  : panelService()  -> refreshes LEDs, drains events.
//   Core1 (panel core)    : panelScanButtons() -> scans I2C inputs, fills queue.
//   Both cores share the I2C bus and the key queue; access is serialized by
//   panelMutex so neither core corrupts the other's transaction or ring buffer.
// =============================================================================

#include "Panel9555.h"
#include "cpuIntel8080.h"   // MEMR_ON..INT_ON masks, shadow vars, VIRTUAL_FRONT_PANEL, printFrontPanel()
#include <pico/mutex.h>

// ---- Cross-core synchronization -------------------------------------------
static mutex_t panelMutex;

// Set true by panelBegin() (Core0) once the I2C bus is initialized; the panel
// core spins on this before its first scan so it never touches an uninitialized
// Wire1.
volatile bool panelReady = false;

// Events detected by the panel core (Core1) and drained/handled by the machine
// core (Core0) so that all Serial output stays on Core0.
volatile bool g_toggleChanged = false;        // a toggle switch changed position
volatile int8_t g_protMsg = -1;               // -1 none, 1 = PROTECT on, 0 = PROTECT off

// ---- PCA9555 low level ------------------------------------------------------
static uint8_t chipAddr[4];
static bool    chipPresent[4] = { false, false, false, false };
static uint16_t outShadow[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };  // active LOW: 1 = off
static bool     dirty[4]     = { false, false, false, false };

static uint8_t pca_read(uint8_t idx, uint8_t reg) {
  if (!chipPresent[idx]) return 0xFF;
  uint8_t r = 0xFF;
  mutex_enter_blocking(&panelMutex);
  PANEL_WIRE.beginTransmission(chipAddr[idx]);
  PANEL_WIRE.write(reg);
  if (PANEL_WIRE.endTransmission(false) != 0) { mutex_exit(&panelMutex); return 0xFF; }
  if (PANEL_WIRE.requestFrom((int)chipAddr[idx], 1, true) != 1) { mutex_exit(&panelMutex); return 0xFF; }
  r = PANEL_WIRE.read();
  mutex_exit(&panelMutex);
  return r;
}

static void pca_write(uint8_t idx, uint8_t reg, uint8_t val) {
  if (!chipPresent[idx]) return;
  mutex_enter_blocking(&panelMutex);
  PANEL_WIRE.beginTransmission(chipAddr[idx]);
  PANEL_WIRE.write(reg);
  PANEL_WIRE.write(val);
  PANEL_WIRE.endTransmission();
  mutex_exit(&panelMutex);
}

// ---- Button scan state ------------------------------------------------------
static bool  g_btnState[BTN_COUNT] = { false };
static uint8_t g_dbnc[BTN_COUNT]   = { 0 };
#define BTN_DEBOUNCE 4

// Memory-protect flag (driven by PROTECT / UNPROTECT buttons; shown on PROT LED).
bool status_prot = false;

// ---- Key queue (command-button presses expressed as emulator keys) -----------
// Single producer (Core1 / panelScanButtons) and single consumer (Core0 /
// consoleRead). Protected by panelMutex.
static uint8_t keyQ[16];
static uint8_t qHead = 0, qTail = 0;
static void enqueueKey(uint8_t k) {
  mutex_enter_blocking(&panelMutex);
  uint8_t n = (qHead + 1) & 0x0F;
  if (n != qTail) { keyQ[qHead] = k; qHead = n; }
  mutex_exit(&panelMutex);
}

// ---- LED "on" evaluation ----------------------------------------------------
static bool ledIsOn(int led) {
  switch (led) {
    case LED_A0 ... LED_A15: return (fpAddressWord >> (led - LED_A0)) & 1;
    case LED_D0 ... LED_D7:  return (fpDataByte   >> (led - LED_D0)) & 1;
    case LED_ST_MEMR:  return fpStatusByte & MEMR_ON;
    case LED_ST_INP:   return fpStatusByte & INP_ON;
    case LED_ST_M1:    return fpStatusByte & M1_ON;
    case LED_ST_OUT:   return fpStatusByte & OUT_ON;
    case LED_ST_HLTA:  return fpStatusByte & HLTA_ON;
    case LED_ST_STACK: return fpStatusByte & STACK_ON;
    case LED_ST_WO:    return fpStatusByte & WO_ON;
    case LED_ST_INT:   return fpStatusByte & INT_ON;
    case LED_WAIT:  return status_wait  != 0;
    case LED_HLDA:  return status_hlda != 0;
    case LED_INTE:  return status_inte != 0;
    case LED_PROT:  return status_prot != 0;
    default: return false;
  }
}

// LEDs are active LOW: a logical "on" is driven as a LOW output.
static void applyLed(int led, bool on) {
  const PanelPin &p = ledMap[led];
  uint8_t  bitpos = (p.port == 0) ? p.bit : (uint8_t)(8 + p.bit);
  uint16_t mask   = (uint16_t)1 << bitpos;
  if (on) outShadow[p.chip] &= ~mask;   // LOW  = lit
  else    outShadow[p.chip] |=  mask;   // HIGH = off
  dirty[p.chip] = true;
}

static void flushLeds(void) {
  for (int c = 0; c < 4; c++) {
    if (!chipPresent[c] || !dirty[c]) continue;
    pca_write(c, PCA9555_OUT0, outShadow[c] & 0xFF);
    pca_write(c, PCA9555_OUT1, (outShadow[c] >> 8) & 0xFF);
    dirty[c] = false;
  }
}

void panelLedsUpdate(void) {
  for (int i = 0; i < LED_COUNT; i++) applyLed(i, ledIsOn(i));
  flushLeds();
}

// ---- Button / switch scan (runs on Core1, the panel core) -------------------
// Reads all four PCA9555 input ports, debounces, and either reflects toggle
// switches into fpAddressToggleWord or enqueues a command key. Event feedback
// (Serial messages / front-panel reprint) is deferred to Core0 via flags so
// that all Serial output happens on a single core.
void panelScanButtons(void) {
  for (int c = 0; c < 4; c++) {            // scan all four chips
    if (!chipPresent[c]) continue;
    uint8_t in0 = pca_read(c, PCA9555_IN0);
    uint8_t in1 = pca_read(c, PCA9555_IN1);
    uint16_t inword = (uint16_t)in0 | ((uint16_t)in1 << 8);
    for (int b = 0; b < BTN_COUNT; b++) {
      if (btnMap[b].chip != (uint8_t)c) continue;
      uint8_t bitpos = (btnMap[b].port == 0) ? btnMap[b].bit : (uint8_t)(8 + btnMap[b].bit);
      bool pressed = ((inword >> bitpos) & 1) == 0;   // active LOW
      if (pressed != g_btnState[b]) {
        if (++g_dbnc[b] >= BTN_DEBOUNCE) {
          g_btnState[b] = pressed;
          if (pressed) {
            if (btnMap[b].twBit != 0xFF) {
              // Toggle switch: reflect its position into fpAddressToggleWord.
              // fpAddressToggleWord is an aligned uint16_t, so the read/write is
              // atomic on the Cortex-M0+; Core0 reads it without a lock.
              if (pressed) fpAddressToggleWord |=  (uint16_t)1 << btnMap[b].twBit;
              else         fpAddressToggleWord &= ~((uint16_t)1 << btnMap[b].twBit);
              g_toggleChanged = true;
            } else if (btnMap[b].cmd != 0) {
              enqueueKey(btnMap[b].cmd);
            } else if (b == BTN_PROTECT) {
              status_prot = true;
              g_protMsg = 1;
            } else if (b == BTN_UNPROTECT) {
              status_prot = false;
              g_protMsg = 0;
            }
          }
        }
      } else {
        g_dbnc[b] = 0;
      }
    }
  }
}

// ---- Event drain (runs on Core0, the machine core) --------------------------
// Handles the feedback that the panel core deferred: reprint the front panel
// when a toggle changed, and echo the PROTECT/UNPROTECT message.
static void panelDrainEvents(void) {
  if (g_toggleChanged) {
    g_toggleChanged = false;
    if (!VIRTUAL_FRONT_PANEL) printFrontPanel();
  }
  if (g_protMsg >= 0) {
    if (g_protMsg) Serial.println(F("+ PROTECT on"));
    else           Serial.println(F("+ PROTECT off"));
    g_protMsg = -1;
  }
}

// ---- Public API ------------------------------------------------------------
void panelBegin(void) {
  mutex_init(&panelMutex);

  for (int i = 0; i < 4; i++) chipAddr[i] = PANEL_CHIP_ADDR[i];

  PANEL_WIRE.setSDA(PANEL_SDA_PIN);
  PANEL_WIRE.setSCL(PANEL_SCL_PIN);
  PANEL_WIRE.begin();
  PANEL_WIRE.setClock(400000);

  // Configure every pin as output (LED) or input (button/switch) and probe each chip.
  for (int c = 0; c < 4; c++) {
    uint16_t dir = 0;                       // 1 = input
    for (int i = 0; i < LED_COUNT; i++)
      if (ledMap[i].chip == (uint8_t)c) dir &= ~((uint16_t)1 << ((ledMap[i].port == 0) ? ledMap[i].bit : 8 + ledMap[i].bit));
    for (int i = 0; i < BTN_COUNT; i++)
      if (btnMap[i].chip == (uint8_t)c) dir |=  ((uint16_t)1 << ((btnMap[i].port == 0) ? btnMap[i].bit : 8 + btnMap[i].bit));

    PANEL_WIRE.beginTransmission(chipAddr[c]);
    PANEL_WIRE.write(PCA9555_CFG0);
    PANEL_WIRE.write((uint8_t)(dir & 0xFF));
    PANEL_WIRE.write((uint8_t)((dir >> 8) & 0xFF));
    chipPresent[c] = (PANEL_WIRE.endTransmission() == 0);
    if (!chipPresent[c]) {
      Serial.print(F("+ Panel PCA9555 0x"));
      if (chipAddr[c] < 0x10) Serial.print('0');
      Serial.print(chipAddr[c], HEX);
      Serial.println(F(" not found (skipped)."));
      continue;
    }
    // All outputs initially HIGH (LEDs off, active LOW); inputs need no init.
    outShadow[c] = 0xFFFF;
    pca_write(c, PCA9555_OUT0, 0xFF);
    pca_write(c, PCA9555_OUT1, 0xFF);
  }
  panelLedsUpdate();
  panelReady = true;   // let the panel core (Core1) begin scanning
}

static unsigned long lastLed = 0;
// Core0 (machine core): refresh the LEDs (throttled) and drain any deferred
// panel events. Button scanning now happens on Core1 (panelScanButtons).
void panelService(void) {
  unsigned long now = millis();
  if ((long)(now - lastLed) >= 12) { lastLed = now; panelLedsUpdate(); }
  panelDrainEvents();
}

bool consoleAvailable(void) {
  mutex_enter_blocking(&panelMutex);
  bool hasKey = (qHead != qTail);
  mutex_exit(&panelMutex);
  return hasKey || (Serial.available() > 0);
}

int consoleRead(void) {
  mutex_enter_blocking(&panelMutex);
  bool hasKey = (qHead != qTail);
  uint8_t k = 0;
  if (hasKey) { k = keyQ[qTail]; qTail = (qTail + 1) & 0x0F; }
  mutex_exit(&panelMutex);
  if (hasKey) return k;
  return Serial.read();
}

byte panelReadSenseByte(bool highlow) {
  return highlow ? (byte)(fpAddressToggleWord >> 8) : (byte)(fpAddressToggleWord & 0xFF);
}
