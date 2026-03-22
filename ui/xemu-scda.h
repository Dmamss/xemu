/*
 * xemu Splinter Cell: Double Agent Game-Specific Patches
 *
 * Detects SCDA by Xbox title ID and applies known workarounds:
 *   - Forces surface_scale = 1 to prevent MLAA anti-aliasing corruption
 *     that occurs at 2x+ scale in single-player mode (xemu issue #949).
 *
 * Title ID source: https://xemu.app/titles/5553005e/
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
#include <stdint.h>
#include <stdbool.h>

/*
 * Splinter Cell: Double Agent Xbox title ID.
 * Confirmed from xemu compatibility database: https://xemu.app/titles/5553005e/
 */
#define SCDA_TITLE_ID 0x5553005EU

/* Returns true if title_id matches SCDA. */
bool xemu_scda_is_title(uint32_t title_id);

/*
 * Polls the currently loaded XBE title ID and applies or removes SCDA
 * patches as needed. Must be called with the Big QEMU Lock (BQL) held,
 * as it reads from emulated guest memory via xemu_get_xbe_info().
 *
 * Internally throttled — safe to call once per vblank.
 */
void xemu_scda_update(void);
