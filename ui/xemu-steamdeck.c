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
     * Audio: disable HRTF (expensive 3D processing) and cap voice-processing
     * workers to 2. The Steam Deck has 8 logical cores (Zen 2, 4c/8t); auto
     * detection (num_workers=0) claims all 8 via SDL_GetNumLogicalCPUCores(),
     * leaving too little headroom for QEMU TCG, NV2A, and Gamescope. 2 workers
     * balances audio quality vs. emulation CPU budget.
     */
    g_config.audio.hrtf = false;
    if (g_config.audio.vp.num_workers == 0) {
        g_config.audio.vp.num_workers = 2;
    }

    /*
     * Display: surface scale must be 1x.
     * Scale >= 2x triggers rendering artifacts (broken MLAA/AA) on several
     * Xbox titles including Splinter Cell Double Agent. Gamescope + FSR
     * handles the upscale to 1280x800.
     */
    g_config.display.quality.surface_scale = 1;

    /*
     * VSync: enabled on Steam Deck.
     * Gamescope runs at 60Hz and frame-doubles xemu output: each xemu frame
     * is displayed twice, producing stable 30fps for 30fps games like SCDA.
     * vsync=false causes the render loop to spin at 200fps+, wasting GPU
     * power and triggering thermal throttling on the Steam Deck APU.
     */
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

void xemu_steamdeck_apply_env_hints(void)
{
    /*
     * RADV graphics pipeline library: Mesa builds pipelines from linkable
     * shader library objects so a draw call doesn't block on a full
     * pipeline compile. Cuts traversal-time stutter on Vulkan renderer.
     * Default-on in Mesa 24+, harmless to set explicitly on older Mesa.
     */
    setenv("RADV_PERFTEST", "gpl", 0);

    /*
     * Mesa GL threading: offloads GL API calls to a worker thread, freeing
     * the nv2a pgraph thread on the OpenGL renderer path. Mesa keeps a
     * per-app allow-list; setting the env var force-enables for xemu.
     */
    setenv("mesa_glthread", "true", 0);

    /*
     * Single-file Mesa shader disk cache: avoids fsync churn from the
     * default multi-file layout and speeds up cache hits at startup.
     */
    setenv("MESA_DISK_CACHE_SINGLE_FILE", "1", 0);

    fprintf(stderr, "[SteamDeck] Mesa/RADV env hints applied\n");
}

void xemu_steamdeck_inject_accel_opts(int *argc, char ***argv)
{
    /* Don't override if the user explicitly passed -accel */
    for (int i = 1; i < *argc; i++) {
        if ((*argv)[i] && strcmp((*argv)[i], "-accel") == 0) {
            return;
        }
    }

    /*
     * The Xbox machine is TCG-only: hw/xbox/xbox.c sets max_cpus=1 and
     * defines no kvm_type callback. Inject -accel tcg,thread=single to:
     *   - skip MTTCG capability probing entirely (saves startup overhead)
     *   - guard against any future QEMU default change that might try MTTCG
     *
     * Note: tb-size is intentionally left at the QEMU default (1 GiB on
     * x86-64). The buffer is BSS/demand-paged, so physical RAM usage equals
     * actual JIT code generated (~5-20 MiB for typical Xbox games) regardless
     * of the limit — tuning it would have no measurable effect.
     */
    int new_argc = *argc + 2;
    char **new_argv = g_new(char *, new_argc + 1);
    new_argv[0] = (*argv)[0];
    new_argv[1] = g_strdup("-accel");
    new_argv[2] = g_strdup("tcg,thread=single");
    for (int i = 1; i < *argc; i++) {
        new_argv[i + 2] = (*argv)[i];
    }
    new_argv[new_argc] = NULL;
    *argc = new_argc;
    *argv = new_argv;
    fprintf(stderr, "[SteamDeck] TCG: thread=single\n");
}
