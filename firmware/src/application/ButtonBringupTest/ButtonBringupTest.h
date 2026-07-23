#pragma once

/**
 * Define-driven side-button bring-up (blocks app when enabled).
 *
 * Enable in Hardware.h:
 *   #define ILSS_TEMP_BUTTON_BRINGUP_TEST 1
 *
 * Then set the knobs below (or override them in Hardware.h before including this).
 * Flash, open serial monitor @ 115200, press left/right and watch levels + events.
 */

#ifndef ILSS_BTN_TEST_ACTIVE_HIGH
/** 0 = pressed when GPIO reads 0 (schematic: to GND). 1 = pressed when 1. */
#define ILSS_BTN_TEST_ACTIVE_HIGH 0
#endif

#ifndef ILSS_BTN_TEST_PULL
/** 0 = floating (no internal pull), 1 = pull-up, 2 = pull-down. */
#define ILSS_BTN_TEST_PULL 1
#endif

#ifndef ILSS_BTN_TEST_SWAP_PINS
/** 1 = treat GPIO9 as left / GPIO8 as right (wiring swap check). */
#define ILSS_BTN_TEST_SWAP_PINS 0
#endif

#ifndef ILSS_BTN_TEST_USE_IOT_BUTTON
/** 1 = also create espressif/button handles and log their events. */
#define ILSS_BTN_TEST_USE_IOT_BUTTON 1
#endif

#ifndef ILSS_BTN_TEST_LONG_MS
#define ILSS_BTN_TEST_LONG_MS 2500
#endif

#ifndef ILSS_BTN_TEST_SHORT_MS
#define ILSS_BTN_TEST_SHORT_MS 500
#endif

#ifndef ILSS_BTN_TEST_POLL_MS
/** How often to dump raw levels even if unchanged. */
#define ILSS_BTN_TEST_POLL_MS 500
#endif

#ifndef ILSS_BTN_TEST_PIN_HUNT
/**
 * 1 = also poll common XIAO GPIOs and log HUNT when any goes active-low.
 * Use when mapped "right" pin never edges — find where the switch actually lands.
 */
#define ILSS_BTN_TEST_PIN_HUNT 0
#endif

/** Never returns. Call from app_main after logger/LowLevel are up. */
void ilss_button_bringup_test_run();
