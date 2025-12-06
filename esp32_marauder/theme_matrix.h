/*
 * theme_matrix.h
 * Matrix/Hacker Green Theme for ESP32 Marauder Pentest Build
 *
 * Inspired by The Matrix - retro terminal hacker aesthetic
 * All greens with phosphor glow effects
 */

#pragma once

#ifndef theme_matrix_h
#define theme_matrix_h

#include "configs.h"

#ifdef MATRIX_THEME

// ============================================================================
// MATRIX COLOR PALETTE (RGB565 format)
// ============================================================================

// Primary Matrix Greens
#define MATRIX_GREEN_BRIGHT   0x07E0  // Pure bright green (255, 255, 0) - main text
#define MATRIX_GREEN          0x07C0  // Standard matrix green
#define MATRIX_GREEN_DARK     0x0400  // Dark green for backgrounds
#define MATRIX_GREEN_DIM      0x0320  // Dim green for inactive items
#define MATRIX_GREEN_GLOW     0x0FE0  // Bright glow effect

// Phosphor variants (CRT monitor effect)
#define PHOSPHOR_GREEN        0x2FE0  // Slight yellow-green tint
#define PHOSPHOR_BRIGHT       0x5FE0  // Bright phosphor
#define PHOSPHOR_FADE         0x0360  // Fading phosphor

// Accent colors (used sparingly)
#define MATRIX_RED            0xF800  // Error/danger - pure red
#define MATRIX_AMBER          0xFC00  // Warnings - amber/orange
#define MATRIX_CYAN           0x07FF  // Info highlights - cyan
#define MATRIX_WHITE          0xFFFF  // High contrast text

// Background shades
#define MATRIX_BLACK          0x0000  // Pure black
#define MATRIX_DARK_BG        0x0020  // Very dark green tint
#define MATRIX_DARKER_BG      0x0010  // Almost black with green

// Terminal-style grays with green tint
#define MATRIX_GRAY_DARK      0x0841  // Dark gray
#define MATRIX_GRAY_MED       0x1082  // Medium gray
#define MATRIX_GRAY_LIGHT     0x2104  // Light gray

// ============================================================================
// THEME OVERRIDE MACROS
// Remap standard colors to matrix palette
// ============================================================================

// Override TFT_eSPI colors when theme is active
#undef TFT_GREEN
#undef TFT_CYAN
#undef TFT_RED
#undef TFT_BLUE
#undef TFT_WHITE
#undef TFT_YELLOW
#undef TFT_ORANGE
#undef TFT_MAGENTA
#undef TFT_PURPLE
#undef TFT_VIOLET

#define TFT_GREEN       MATRIX_GREEN_BRIGHT
#define TFT_CYAN        MATRIX_GREEN         // Make cyan greenish
#define TFT_RED         MATRIX_RED
#define TFT_BLUE        MATRIX_GREEN_DIM     // Blue becomes dim green
#define TFT_WHITE       MATRIX_GREEN_BRIGHT
#define TFT_YELLOW      PHOSPHOR_GREEN
#define TFT_ORANGE      MATRIX_AMBER
#define TFT_MAGENTA     MATRIX_GREEN_GLOW
#define TFT_PURPLE      MATRIX_GREEN
#define TFT_VIOLET      PHOSPHOR_BRIGHT

// ============================================================================
// UI ELEMENT COLORS
// ============================================================================

// Menu colors
#define MENU_BG_COLOR         MATRIX_BLACK
#define MENU_TEXT_COLOR       MATRIX_GREEN_BRIGHT
#define MENU_SELECTED_BG      MATRIX_GREEN_DARK
#define MENU_SELECTED_TEXT    MATRIX_GREEN_BRIGHT
#define MENU_INACTIVE_TEXT    MATRIX_GREEN_DIM
#define MENU_BORDER_COLOR     MATRIX_GREEN

// Status bar
#define STATUSBAR_BG          MATRIX_DARK_BG
#define STATUSBAR_TEXT        MATRIX_GREEN
#define STATUSBAR_ICON        MATRIX_GREEN_BRIGHT

// Buttons
#define BUTTON_BG             MATRIX_BLACK
#define BUTTON_BORDER         MATRIX_GREEN
#define BUTTON_TEXT           MATRIX_GREEN_BRIGHT
#define BUTTON_PRESSED_BG     MATRIX_GREEN_DARK
#define BUTTON_PRESSED_BORDER MATRIX_GREEN_BRIGHT

// Graphs and charts
#define GRAPH_LINE            MATRIX_GREEN_BRIGHT
#define GRAPH_FILL            MATRIX_GREEN_DARK
#define GRAPH_GRID            MATRIX_GREEN_DIM
#define GRAPH_AXIS            MATRIX_GREEN

// Terminal output
#define TERMINAL_BG           MATRIX_BLACK
#define TERMINAL_TEXT         MATRIX_GREEN
#define TERMINAL_PROMPT       MATRIX_GREEN_BRIGHT
#define TERMINAL_ERROR        MATRIX_RED
#define TERMINAL_WARNING      MATRIX_AMBER
#define TERMINAL_SUCCESS      MATRIX_GREEN_GLOW

// ============================================================================
// CATEGORY COLORS (for pentest modules)
// ============================================================================

// WiFi category - bright green
#define WIFI_COLOR            MATRIX_GREEN_BRIGHT

// Bluetooth category - slightly dimmer
#define BT_COLOR              MATRIX_GREEN

// IR category - phosphor glow
#define IR_COLOR              PHOSPHOR_BRIGHT

// Sub-GHz category - amber accent
#define SUBGHZ_COLOR          MATRIX_AMBER

// RFID category - cyan accent
#define RFID_COLOR            MATRIX_CYAN

// GPS category - standard green
#define GPS_COLOR             MATRIX_GREEN

// Device/Settings - dim green
#define DEVICE_COLOR          MATRIX_GREEN_DIM

// Attack modes - red accent
#define ATTACK_COLOR          MATRIX_RED

// Sniffers - glow green
#define SNIFFER_COLOR         MATRIX_GREEN_GLOW

// ============================================================================
// SECURITY LEVEL COLORS
// ============================================================================

#define SEC_CRITICAL_COLOR    MATRIX_RED
#define SEC_LOW_COLOR         MATRIX_AMBER
#define SEC_MEDIUM_COLOR      MATRIX_GREEN
#define SEC_HIGH_COLOR        MATRIX_GREEN_BRIGHT
#define SEC_UNKNOWN_COLOR     MATRIX_GRAY_MED

// ============================================================================
// SIGNAL STRENGTH COLORS
// ============================================================================

#define RSSI_EXCELLENT        MATRIX_GREEN_BRIGHT  // > -50 dBm
#define RSSI_GOOD             MATRIX_GREEN         // -50 to -60
#define RSSI_FAIR             PHOSPHOR_GREEN       // -60 to -70
#define RSSI_WEAK             MATRIX_AMBER         // -70 to -80
#define RSSI_POOR             MATRIX_RED           // < -80

// ============================================================================
// ANIMATION EFFECTS
// ============================================================================

// Matrix rain effect colors (for boot screen or idle)
#define RAIN_BRIGHT           MATRIX_GREEN_BRIGHT
#define RAIN_MED              MATRIX_GREEN
#define RAIN_DIM              MATRIX_GREEN_DIM
#define RAIN_FADE             MATRIX_GREEN_DARK

// Scanline effect
#define SCANLINE_COLOR        MATRIX_GREEN_DIM
#define SCANLINE_BRIGHT       MATRIX_GREEN

// ============================================================================
// ASCII ART - MARAUDER NEO SPLASH SCREEN
// Grinch eyes for that mischievous hacker vibe
// ============================================================================

// Main splash screen - Marauder Neo with Grinch eyes
const char MARAUDER_NEO_SPLASH[] PROGMEM = R"(

        ██╗     ██╗
       ████╗   ████║      MARAUDER
      ██╔═██╗██╔═██║        NEO
     ██║  ╚███╔╝ ██║
    ██╔╝   ╚█╔╝  ╚██╗   ┌─────────────┐
   ██╔╝    ███    ╚██╗  │ PENTEST OS  │
  ██╔╝    ██╔██    ╚██╗ └─────────────┘
  ╚═╝     ╚═╝╚═╝    ╚═╝

)";

// Grinch eyes - mischievous look
const char GRINCH_EYES[] PROGMEM = R"(
    ╔══╗         ╔══╗
   ║ ▄▀ ║       ║ ▀▄ ║
   ║█▀  ║       ║  ▀█║
    ╚══╝    ▄    ╚══╝
            █
         ▀▀▀▀▀
)";

// Alternative compact Grinch eyes for smaller screens
const char GRINCH_EYES_SMALL[] PROGMEM = R"(
  /▀▄\   /▄▀\
  \▄▀/   \▀▄/
     \___/
)";

// Full Neo splash with eyes
const char NEO_FULL_SPLASH[] PROGMEM = R"(
   ╔═══════════════════════════════╗
   ║                               ║
   ║      /▀▄\       /▄▀\          ║
   ║      \▄▀/       \▀▄/          ║
   ║          \▄▄▄▄▄/              ║
   ║                               ║
   ║   ███╗   ██╗███████╗ ██████╗  ║
   ║   ████╗  ██║██╔════╝██╔═══██╗ ║
   ║   ██╔██╗ ██║█████╗  ██║   ██║ ║
   ║   ██║╚██╗██║██╔══╝  ██║   ██║ ║
   ║   ██║ ╚████║███████╗╚██████╔╝ ║
   ║   ╚═╝  ╚═══╝╚══════╝ ╚═════╝  ║
   ║                               ║
   ║   M A R A U D E R   v2.0      ║
   ║   [ PENTEST EDITION ]         ║
   ╚═══════════════════════════════╝
)";

// Animated eye frames for blinking effect
const char EYE_OPEN[] PROGMEM = R"(
  /▀▄\   /▄▀\
)";

const char EYE_HALF[] PROGMEM = R"(
  /──\   /──\
)";

const char EYE_CLOSED[] PROGMEM = R"(
  \__/   \__/
)";

// Hacker taglines - randomly shown
const char* const HACKER_TAGLINES[] PROGMEM = {
    "Wake up, Neo...",
    "The Matrix has you...",
    "Follow the white rabbit.",
    "There is no spoon.",
    "I know kung fu.",
    "Free your mind.",
    "Welcome to the real world.",
    "What is the Matrix?",
    "Knock knock, Neo.",
    "You take the red pill..."
};
const uint8_t NUM_TAGLINES = 10;

// Boot sequence messages
const char MATRIX_BOOT_MSG[] PROGMEM = R"(
> NEURAL INTERFACE ONLINE...
> LOADING ATTACK VECTORS...
  [■■■■■■■■■■] WiFi Stack
  [■■■■■■■■■■] Bluetooth
  [■■■■■■■■■■] IR Module
  [■■■■■■■■■■] Sub-GHz Radio
  [■■■■■■■■■■] RFID Reader
> ENTERING THE MATRIX...
> ACCESS GRANTED
)";

// Simple banner for serial output
const char MATRIX_BANNER[] PROGMEM = R"(

  ███╗   ███╗ █████╗ ██████╗  █████╗ ██╗   ██╗██████╗ ███████╗██████╗
  ████╗ ████║██╔══██╗██╔══██╗██╔══██╗██║   ██║██╔══██╗██╔════╝██╔══██╗
  ██╔████╔██║███████║██████╔╝███████║██║   ██║██║  ██║█████╗  ██████╔╝
  ██║╚██╔╝██║██╔══██║██╔══██╗██╔══██║██║   ██║██║  ██║██╔══╝  ██╔══██╗
  ██║ ╚═╝ ██║██║  ██║██║  ██║██║  ██║╚██████╔╝██████╔╝███████╗██║  ██║
  ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝
                           ███╗   ██╗███████╗ ██████╗
                           ████╗  ██║██╔════╝██╔═══██╗
                           ██╔██╗ ██║█████╗  ██║   ██║
                           ██║╚██╗██║██╔══╝  ██║   ██║
                           ██║ ╚████║███████╗╚██████╔╝
                           ╚═╝  ╚═══╝╚══════╝ ╚═════╝
                      ╔═══════════════════════════════╗
                      ║   P E N T E S T   E D I T I O N   ║
                      ╚═══════════════════════════════╝

)

// ============================================================================
// HELPER MACROS
// ============================================================================

// Convert RGB to RGB565
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Dim a color (reduce brightness by ~50%)
#define DIM_COLOR(c) (((c) >> 1) & 0x7BEF)

// Brighten a color (increase towards white)
#define BRIGHT_COLOR(c) ((c) | 0x8410)

#endif // MATRIX_THEME
#endif // theme_matrix_h
