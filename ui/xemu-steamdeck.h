/*
 * xemu Steam Deck Platform Detection and Optimization
 *
 * Detects Steam Deck hardware and applies platform-specific defaults
 * for optimal performance and display compatibility with Gamescope/FSR.
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

#pragma once
#include <stdbool.h>

/*
 * Returns true if running on a Steam Deck (detects via SteamDeck env var
 * set by SteamOS Game Mode, or via DMI product name).
 */
bool xemu_is_steam_deck(void);

/*
 * Applies Steam Deck performance defaults to g_config.
 * Call after xemu_settings_load() but before display initialization.
 *
 * Performance settings (always applied):
 *   - perf.cache_shaders = true
 *   - perf.hard_fpu = true
 *   - audio.hrtf = false     (expensive; disabled for CPU headroom)
 *   - audio.vp.num_workers = 4  (AMD Zen 2 has 4 physical cores)
 *   - display.quality.surface_scale = 1  (>= 2x breaks MLAA on some games)
 *   - display.window.vsync = true
 *
 * Display settings (applied if still at factory defaults):
 *   - display.window.fullscreen_on_startup = true
 *   - display.window.startup_size = 640x480  (Xbox native; FSR upscales to 1280x800)
 */
void xemu_steamdeck_apply_defaults(void);
