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

#ifndef HW_XBOX_NV2A_PGRAPH_VK_SHADER_CACHE_H
#define HW_XBOX_NV2A_PGRAPH_VK_SHADER_CACHE_H

#include "renderer.h"

/*
 * SPIR-V cache — one file per shader module, keyed by node hash.
 * Directory: {xemu_settings_get_base_path()}shaders/vk/{hash>>48}/{hash}
 */
void pgraph_vk_spirv_cache_init(void);
GByteArray *pgraph_vk_spirv_cache_lookup(PGRAPHVkState *r, uint64_t hash);
void pgraph_vk_spirv_cache_store(PGRAPHVkState *r, uint64_t hash,
                                 GByteArray *spirv);

/*
 * VkPipelineCache persistence — single binary file saved on shutdown,
 * loaded on startup.
 * File: {xemu_settings_get_base_path()}vk_pipeline_cache.bin
 */
void pgraph_vk_pipeline_cache_load(PGRAPHState *pg);
void pgraph_vk_pipeline_cache_save(PGRAPHState *pg);

#endif /* HW_XBOX_NV2A_PGRAPH_VK_SHADER_CACHE_H */
