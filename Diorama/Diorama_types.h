#pragma once
#include <stdint.h>

// ── Enum: UI pages ──────────────────────────────────────────────────────────
enum Page { P_LIGHTS = 0, P_AUDIO = 1 };

// ── Enum: Lighting sub-modes ────────────────────────────────────────────────
enum Mode { M_BASIC = 0, M_COLOR = 1, M_SOUND = 2, M_ADAPT = 3 };

// ── Enum: Active drag target ────────────────────────────────────────────────
enum Drag { D_NONE = 0, D_BR, D_HUE, D_SAT, D_SENS, D_VOL };

// ── Struct: 8-bit RGB colour ────────────────────────────────────────────────
struct RGB { uint8_t r, g, b; };

// ── Enum: Gate state machine ────────────────────────────────────────────────
enum GateState {
  GS_WAITING  = 0,   // gate closed, waiting for biometric
  GS_SCANNING = 1,   // fingerprint sensor reading
  GS_OPEN     = 2,   // gate open, UI enabled
  GS_GOODBYE  = 3    // gate just closed, showing goodbye message
};
