/*
 * xemu Splinter Cell: Double Agent Game-Specific Patches
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
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "xemu-settings.h"
#include "xemu-scda.h"
#include "../xemu-xbe.h"

/* How many vblanks between title ID polls (~1 second at 60Hz) */
#define SCDA_CHECK_INTERVAL 60

static uint32_t s_current_title_id = 0;
static int s_saved_surface_scale = 1;
static int s_update_counter = 0;

bool xemu_scda_is_title(uint32_t title_id)
{
    return title_id == SCDA_TITLE_ID;
}

static void scda_apply_patches(void)
{
    /*
     * Force surface_scale = 1x.
     *
     * SCDA uses Morphological Anti-Aliasing (MLAA) in single-player mode.
     * At scale >= 2x, the MLAA shader reads incorrect data from the upscaled
     * framebuffer, producing severe aliasing and color artifacts (xemu #949).
     * The game looks better at 1x + FSR upscaling than at 2x without FSR.
     *
     * Note: widescreen is intentionally NOT enabled here — the xemu widescreen
     * patch causes display stretching in SCDA (xemu issue #1598).
     */
    s_saved_surface_scale = g_config.display.quality.surface_scale;
    g_config.display.quality.surface_scale = 1;

    fprintf(stderr,
            "[SCDA] Patches applied: surface_scale=1 (MLAA fix, title_id=0x%08x)\n",
            SCDA_TITLE_ID);
}

static void scda_reset_patches(void)
{
    g_config.display.quality.surface_scale = s_saved_surface_scale;
    fprintf(stderr, "[SCDA] Patches reset (surface_scale restored to %d)\n",
            s_saved_surface_scale);
}

void xemu_scda_update(void)
{
    /* Throttle: poll title ID once per second, not every vblank */
    if (++s_update_counter < SCDA_CHECK_INTERVAL) {
        return;
    }
    s_update_counter = 0;

    struct xbe *xbe = xemu_get_xbe_info();
    uint32_t title_id = (xbe && xbe->cert) ? xbe->cert->m_titleid : 0;

    if (title_id == s_current_title_id) {
        return; /* no change */
    }

    /* Title changed — reset patches from previous game first */
    if (xemu_scda_is_title(s_current_title_id)) {
        scda_reset_patches();
    }

    s_current_title_id = title_id;

    /* Apply patches if the new game is SCDA */
    if (xemu_scda_is_title(title_id)) {
        scda_apply_patches();
    }
}
