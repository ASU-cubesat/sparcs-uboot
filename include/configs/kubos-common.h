/*
* Copyright (C) 2017 Kubos Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
 * These are the settings needed for the Kubos Software Update/Recovery system.
 * Everything defined here is expected to be common to all boards. There are some
 * options that must be defined in the board's configuration file because they are
 * not guaranteed to be portable:
 *
 * - DFU_ALT_INFO_MMC - Defines the files/partitions which may be replaced in MMC
 *                      storage
 * - DFU_ALT_INFO_NOR - Same as above, but for NOR flash storage
 * - CONFIG_SYS_DFU_DATA_BUF_SIZE - File transfer chunk size (for raw or file transfer. EXT4, FAT, etc)
 * - CONFIG_SYS_DFU_MAX_FILE_SIZE - Max size for a single file (partition transfer)
 * - KUBOS_UPGRADE_DEVICE  - MMC device which has the upgrade partition
 * - KUBOS_UPGRADE_PART    - Partition where the upgrade files are located
 * - KUBOS_UPGRADE_STORAGE - SDRAM address to be used for temporary storage while upgrading files
 */

#pragma once

#ifdef CONFIG_UPDATE_KUBOS
#define CONFIG_BOOTCOUNT_LIMIT
#define CONFIG_BOOTCOUNT_ENV

/* Update Definitions */
#define KUBOS_CURR_VERSION "kubos_curr_version"
#define KUBOS_PREV_VERSION "kubos_prev_version"
#define KUBOS_CURR_TRIED   "kubos_curr_tried"
#define KUBOS_BASE         "update-pack.itb"
#define KUBOS_UPDATE_FILE  "kubos_updatefile"

#define KUBOS_UPDATE_ARGS \
	"altbootcmd=run bootcmd\0" \
	"recovery_available=1\0" \
    "bootlimit=3\0" \
	KUBOS_CURR_VERSION "=" KUBOS_BASE "\0" \
	KUBOS_PREV_VERSION "=" KUBOS_BASE "\0" \
	KUBOS_CURR_TRIED "=0\0" \
	DFU_ALT_INFO_MMC \
	DFU_ALT_INFO_NOR

#else

#define KUBOS_UPDATE_ARGS ""

#endif /* CONFIG_UPDATE_KUBOS */
