/*
 * Geforce NV2A PGRAPH Vulkan Renderer — Shader Disk Cache
 *
 * Copyright (c) 2024-2025 xemu contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/mstring.h"
#include "ui/xemu-settings.h"
#include "xemu-version.h"
#include "renderer.h"
#include "shader_cache.h"

/* Magic numbers for cache file identification */
#define SPIRV_CACHE_MAGIC   0x584D5553U  /* "XMUS" */
#define PIPELINE_CACHE_MAGIC 0x584D5556U /* "XMUV" */

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static char *spirv_get_dir(uint64_t hash)
{
    return g_strdup_printf("%sshaders/vk/%04" PRIx16,
                           xemu_settings_get_base_path(),
                           (uint16_t)(hash >> 48));
}

static char *spirv_get_path(uint64_t hash)
{
    char *dir = spirv_get_dir(hash);
    char *path = g_strdup_printf("%s/%014" PRIx64, dir, hash & 0x3FFFFFFFFFFFULL);
    g_free(dir);
    return path;
}

static char *pipeline_cache_get_path(void)
{
    return g_strdup_printf("%svk_pipeline_cache.bin",
                           xemu_settings_get_base_path());
}

/* -------------------------------------------------------------------------
 * SPIR-V cache
 * ---------------------------------------------------------------------- */

void pgraph_vk_spirv_cache_init(void)
{
    char *dir = g_strdup_printf("%sshaders/vk", xemu_settings_get_base_path());
    qemu_mkdir(dir);
    g_free(dir);
}

/*
 * Try to load a cached SPIR-V binary for the given hash.
 * Returns a newly-allocated GByteArray on success, NULL on miss or error.
 *
 * File layout (all little-endian):
 *   uint32_t  magic          = SPIRV_CACHE_MAGIC
 *   uint32_t  vendor_id
 *   uint32_t  device_id
 *   uint32_t  driver_version
 *   uint64_t  xemu_version_len  (includes NUL)
 *   char      xemu_version[xemu_version_len]
 *   uint64_t  spirv_size
 *   uint8_t   spirv[spirv_size]
 */
GByteArray *pgraph_vk_spirv_cache_lookup(PGRAPHVkState *r, uint64_t hash)
{
    char *path = spirv_get_path(hash);
    FILE *f = qemu_fopen(path, "rb");
    g_free(path);
    if (!f) {
        return NULL;
    }

    GByteArray *spirv = NULL;

#define READ_OR_FAIL(dst, sz) \
    do { \
        if (fread((dst), (sz), 1, f) != 1) goto fail; \
    } while (0)

    uint32_t magic;
    READ_OR_FAIL(&magic, sizeof(magic));
    if (magic != SPIRV_CACHE_MAGIC) {
        goto fail;
    }

    uint32_t vendor_id, device_id, driver_version;
    READ_OR_FAIL(&vendor_id,      sizeof(vendor_id));
    READ_OR_FAIL(&device_id,      sizeof(device_id));
    READ_OR_FAIL(&driver_version, sizeof(driver_version));

    if (vendor_id      != r->device_props.vendorID  ||
        device_id      != r->device_props.deviceID  ||
        driver_version != r->device_props.driverVersion) {
        goto fail;
    }

    uint64_t xemu_version_len;
    READ_OR_FAIL(&xemu_version_len, sizeof(xemu_version_len));
    if (xemu_version_len < 1 || xemu_version_len > 256) {
        goto fail;
    }
    char *cached_version = g_malloc(xemu_version_len);
    if (fread(cached_version, xemu_version_len, 1, f) != 1) {
        g_free(cached_version);
        goto fail;
    }
    cached_version[xemu_version_len - 1] = '\0';
    bool version_ok = (strcmp(cached_version, xemu_version) == 0);
    g_free(cached_version);
    if (!version_ok) {
        goto fail;
    }

    uint64_t spirv_size;
    READ_OR_FAIL(&spirv_size, sizeof(spirv_size));
    if (spirv_size == 0 || spirv_size > 64 * 1024 * 1024) {
        goto fail;
    }

    guint8 *data = g_malloc(spirv_size);
    if (fread(data, spirv_size, 1, f) != 1) {
        g_free(data);
        goto fail;
    }

    spirv = g_byte_array_new_take(data, spirv_size);

#undef READ_OR_FAIL

fail:
    fclose(f);
    return spirv;
}

void pgraph_vk_spirv_cache_store(PGRAPHVkState *r, uint64_t hash,
                                 GByteArray *spirv)
{
    if (!spirv || spirv->len == 0) {
        return;
    }

    char *dir = spirv_get_dir(hash);
    qemu_mkdir(dir);
    g_free(dir);

    char *path = spirv_get_path(hash);
    FILE *f = qemu_fopen(path, "wb");
    g_free(path);
    if (!f) {
        return;
    }

#define WRITE_OR_FAIL(src, sz) \
    do { \
        if (fwrite((src), (sz), 1, f) != 1) { fclose(f); return; } \
    } while (0)

    uint32_t magic = SPIRV_CACHE_MAGIC;
    WRITE_OR_FAIL(&magic, sizeof(magic));

    uint32_t vendor_id      = r->device_props.vendorID;
    uint32_t device_id      = r->device_props.deviceID;
    uint32_t driver_version = r->device_props.driverVersion;
    WRITE_OR_FAIL(&vendor_id,      sizeof(vendor_id));
    WRITE_OR_FAIL(&device_id,      sizeof(device_id));
    WRITE_OR_FAIL(&driver_version, sizeof(driver_version));

    uint64_t xemu_version_len = (uint64_t)(strlen(xemu_version) + 1);
    WRITE_OR_FAIL(&xemu_version_len, sizeof(xemu_version_len));
    WRITE_OR_FAIL(xemu_version, xemu_version_len);

    uint64_t spirv_size = (uint64_t)spirv->len;
    WRITE_OR_FAIL(&spirv_size, sizeof(spirv_size));
    WRITE_OR_FAIL(spirv->data, spirv->len);

#undef WRITE_OR_FAIL

    fclose(f);
}

/* -------------------------------------------------------------------------
 * VkPipelineCache persistence
 * ---------------------------------------------------------------------- */

/*
 * File layout (all little-endian):
 *   uint32_t  magic = PIPELINE_CACHE_MAGIC
 *   uint32_t  vendor_id
 *   uint32_t  device_id
 *   uint32_t  driver_version
 *   uint8_t   pipeline_cache_uuid[VK_UUID_SIZE]
 *   uint64_t  xemu_version_len  (includes NUL)
 *   char      xemu_version[xemu_version_len]
 *   uint64_t  data_size
 *   uint8_t   data[data_size]
 */
void pgraph_vk_pipeline_cache_load(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    char *path = pipeline_cache_get_path();
    FILE *f = qemu_fopen(path, "rb");
    g_free(path);
    if (!f) {
        return;
    }

#define READ_OR_FAIL(dst, sz) \
    do { if (fread((dst), (sz), 1, f) != 1) goto fail; } while (0)

    uint32_t magic;
    READ_OR_FAIL(&magic, sizeof(magic));
    if (magic != PIPELINE_CACHE_MAGIC) {
        goto fail;
    }

    uint32_t vendor_id, device_id, driver_version;
    READ_OR_FAIL(&vendor_id,      sizeof(vendor_id));
    READ_OR_FAIL(&device_id,      sizeof(device_id));
    READ_OR_FAIL(&driver_version, sizeof(driver_version));

    if (vendor_id      != r->device_props.vendorID  ||
        device_id      != r->device_props.deviceID  ||
        driver_version != r->device_props.driverVersion) {
        goto fail;
    }

    uint8_t uuid[VK_UUID_SIZE];
    READ_OR_FAIL(uuid, VK_UUID_SIZE);
    if (memcmp(uuid, r->device_props.pipelineCacheUUID, VK_UUID_SIZE) != 0) {
        goto fail;
    }

    uint64_t xemu_version_len;
    READ_OR_FAIL(&xemu_version_len, sizeof(xemu_version_len));
    if (xemu_version_len < 1 || xemu_version_len > 256) {
        goto fail;
    }
    char *cached_version = g_malloc(xemu_version_len);
    if (fread(cached_version, xemu_version_len, 1, f) != 1) {
        g_free(cached_version);
        goto fail;
    }
    cached_version[xemu_version_len - 1] = '\0';
    bool version_ok = (strcmp(cached_version, xemu_version) == 0);
    g_free(cached_version);
    if (!version_ok) {
        goto fail;
    }

    uint64_t data_size;
    READ_OR_FAIL(&data_size, sizeof(data_size));
    if (data_size == 0 || data_size > 256 * 1024 * 1024) {
        goto fail;
    }

    void *data = g_malloc(data_size);
    if (fread(data, data_size, 1, f) != 1) {
        g_free(data);
        goto fail;
    }

    /* Recreate the pipeline cache with preloaded data */
    vkDestroyPipelineCache(r->device, r->vk_pipeline_cache, NULL);
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = (size_t)data_size,
        .pInitialData = data,
    };
    VkResult result = vkCreatePipelineCache(r->device, &cache_info, NULL,
                                            &r->vk_pipeline_cache);
    g_free(data);

    if (result == VK_SUCCESS) {
        fprintf(stderr, "[VK] Pipeline cache loaded from disk\n");
    }
    /* If the driver rejects the data, vk_pipeline_cache is VK_NULL_HANDLE.
     * Callers must handle this; we fall through to recreate an empty one. */

#undef READ_OR_FAIL

    fclose(f);
    return;

fail:
    fclose(f);
}

void pgraph_vk_pipeline_cache_save(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    if (r->vk_pipeline_cache == VK_NULL_HANDLE) {
        return;
    }

    size_t data_size = 0;
    if (vkGetPipelineCacheData(r->device, r->vk_pipeline_cache,
                               &data_size, NULL) != VK_SUCCESS) {
        return;
    }
    if (data_size == 0) {
        return;
    }

    void *data = g_malloc(data_size);
    if (vkGetPipelineCacheData(r->device, r->vk_pipeline_cache,
                               &data_size, data) != VK_SUCCESS) {
        g_free(data);
        return;
    }

    char *path = pipeline_cache_get_path();
    FILE *f = qemu_fopen(path, "wb");
    g_free(path);
    if (!f) {
        g_free(data);
        return;
    }

#define WRITE_OR_FAIL(src, sz) \
    do { \
        if (fwrite((src), (sz), 1, f) != 1) { \
            fclose(f); g_free(data); return; \
        } \
    } while (0)

    uint32_t magic = PIPELINE_CACHE_MAGIC;
    WRITE_OR_FAIL(&magic, sizeof(magic));

    uint32_t vendor_id      = r->device_props.vendorID;
    uint32_t device_id      = r->device_props.deviceID;
    uint32_t driver_version = r->device_props.driverVersion;
    WRITE_OR_FAIL(&vendor_id,      sizeof(vendor_id));
    WRITE_OR_FAIL(&device_id,      sizeof(device_id));
    WRITE_OR_FAIL(&driver_version, sizeof(driver_version));
    WRITE_OR_FAIL(r->device_props.pipelineCacheUUID, VK_UUID_SIZE);

    uint64_t xemu_version_len = (uint64_t)(strlen(xemu_version) + 1);
    WRITE_OR_FAIL(&xemu_version_len, sizeof(xemu_version_len));
    WRITE_OR_FAIL(xemu_version, xemu_version_len);

    uint64_t sz = (uint64_t)data_size;
    WRITE_OR_FAIL(&sz, sizeof(sz));
    WRITE_OR_FAIL(data, data_size);

#undef WRITE_OR_FAIL

    fclose(f);
    g_free(data);

    fprintf(stderr, "[VK] Pipeline cache saved to disk (%zu bytes)\n",
            data_size);
}
