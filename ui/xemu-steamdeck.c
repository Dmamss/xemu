/*
 * xemu Steam Deck Platform Detection and Optimization
 *
 * Copyright (c) 2024 xemu contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "xemu-settings.h"
#include "xemu-steamdeck.h"

bool xemu_is_steam_deck(void)
{
    /* Primary detection: SteamOS sets SteamDeck=1 in Game Mode */
    const char *sd_env = getenv("SteamDeck");
    if (sd_env && strcmp(sd_env, "1") == 0) {
        return true;
    }

    /* Fallback: DMI product name (works in Desktop Mode and on other Linux distros) */
    FILE *f = fopen("/sys/class/dmi/id/product_name", "r");
    if (f) {
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        if (n > 0 && strncmp(buf, "Steam Deck", 10) == 0) {
            return true;
        }
    }

    return false;
}

void xemu_steamdeck_apply_defaults(void)
{
    fprintf(stderr, "[SteamDeck] Applying Steam Deck optimizations\n");

    /*
     * Performance: always enforce on Steam Deck.
     * These are safe to override regardless of user prefs.
     */
    g_config.perf.cache_shaders = true;
    g_config.perf.hard_fpu = true;

    /*
     * Audio: disable HRTF (expensive 3D processing not needed for TV/handheld
     * gaming) and use 4 voice-processing workers matching Zen 2 physical cores.
     */
    g_config.audio.hrtf = false;
    g_config.audio.vp.num_workers = 4;

    /*
     * Display: surface scale must be 1x.
     * Scale >= 2x triggers rendering artifacts (broken MLAA/AA) on several
     * Xbox titles. Gamescope + FSR handles the upscale to 1280x800.
     */
    g_config.display.quality.surface_scale = 1;
    g_config.display.window.vsync = true;

    /*
     * Display: fullscreen on startup.
     * Only apply if still at the default (false) so the user can override.
     */
    if (!g_config.display.window.fullscreen_on_startup) {
        g_config.display.window.fullscreen_on_startup = true;
    }

    /*
     * Display: startup resolution.
     * Use 640x480 (Xbox native output) so Gamescope FSR can upscale cleanly
     * to 1280x800 — better image quality than starting at a larger resolution.
     * Only apply if still at the 1280x960 factory default.
     */
    if (g_config.display.window.startup_size ==
            CONFIG_DISPLAY_WINDOW_STARTUP_SIZE_1280X960) {
        g_config.display.window.startup_size =
            CONFIG_DISPLAY_WINDOW_STARTUP_SIZE_640X480;
    }
}
