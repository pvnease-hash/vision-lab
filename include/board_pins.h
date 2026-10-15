// board_pins.h - the ONLY place hardware pins are defined.
//
// Provenance of every value below is stated. Pins marked UNVERIFIED were not
// confirmed against the board or the schematic in the 2026-10-14 sessions and
// MUST be metered before this firmware is flashed.
#pragma once

// ---------------------------------------------------------------------------
// Shared SPI bus (main LCD + both eyes)
// ---------------------------------------------------------------------------
// VERIFIED: Waveshare's own reference design for this panel uses GPIO0 as the
// main LCD SCLK (EXAMPLE_PIN_NUM_SCLK 0). The board booted on it all session.
// Note: GPIO0 is NOT a strapping pin on the ESP32-C6 (those are 8/9/15, and
// arguably 4/5 as MTMS/MTDI) - that is an ESP32-classic fact and does not
// apply here.
#define PIN_SPI_SCLK 0

// UNVERIFIED: eye module MOSI/SDA. Not captured in the wiring audit.
#define PIN_SPI_MOSI 1
#define BOARD_PINS_UNVERIFIED_MOSI 1

// The eyes are write-only; GC9D01 has no MISO line.
#define PIN_SPI_MISO GFX_NOT_DEFINED

// ---------------------------------------------------------------------------
// Eye panels - dual chip-select on the shared bus
// ---------------------------------------------------------------------------
// VERIFIED by schematic audit: CS_L and CS_R land on GPIO4 and GPIO5, which are
// MTMS/MTDI. Some C6 references list these as strapping pins; they idle high,
// which is the level strapping samples. Harmless for damage, but a suspect if
// boot behaviour ever looks wrong.
#define PIN_EYE_CS_L 4
#define PIN_EYE_CS_R 5

// UNVERIFIED: DC is shared by both panels on the Waveshare module.
#define PIN_EYE_DC 2
#define BOARD_PINS_UNVERIFIED_DC 1

// UNVERIFIED: RST1/RST2 and BL1/BL2 are INDIVIDUAL flying leads on the
// SH1.0-to-Dupont cable - the product photo confirmed they are not tied on the
// module. A floating RST means the controller never leaves reset; a floating BL
// means no light regardless of what is drawn. Either alone explains dark panels.
// Set these to the GPIOs you actually land them on.
#define PIN_EYE_RST_L 6
#define PIN_EYE_RST_R 7
#define PIN_EYE_BL_L  10
#define PIN_EYE_BL_R  11
#define BOARD_PINS_UNVERIFIED_RST_BL 1

// ---------------------------------------------------------------------------
// Panel geometry
// ---------------------------------------------------------------------------
// GC9D01 is the 0.71" 160x160 round controller (Waveshare 0.71" dual eye
// module). GC9A01 is the 1.28" 240x240 part and is a DIFFERENT controller - a
// GC9A01 init table on a GC9D01 panel yields a wrong init sequence and wrong
// framebuffer geometry, which presents as a lit panel with a detail-free glow.
#define EYE_WIDTH  160
#define EYE_HEIGHT 160

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
#define JARVIS_AMBER 0xFFB454  // sensitive fields only

#if defined(BOARD_PINS_UNVERIFIED_MOSI) || defined(BOARD_PINS_UNVERIFIED_DC) || defined(BOARD_PINS_UNVERIFIED_RST_BL)
#warning "board_pins.h contains UNVERIFIED pin assignments - meter them before flashing"
#endif
