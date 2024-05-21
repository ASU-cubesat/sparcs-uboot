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
 *
 * pumpkin-mbm2.h
 *
 * Configuration file for the Pumpkin Motherboard Module 2, using a Beaglebone Black
 * as the OBC.
 */

#pragma once

#include "am335x_evm.h"
#include "kubos-common.h"

/* Undo things we don't want to include from the base Beaglebone Black configuration */
#undef CONFIG_SYS_LDSCRIPT /* For NOR flash, which we (and the BBB) don't support */
#undef CONFIG_BOOTCOMMAND
#undef BOOT_TARGET_DEVICES
#undef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_ENV_IS_IN_MMC
#undef CONFIG_ENV_IS_IN_FAT
#undef CONFIG_ENV_IS_NOWHERE
#undef CONFIG_ENV_SIZE
#undef DFU_ALT_INFO_MMC
#undef DFU_ALT_INFO_NOR
#undef CONFIG_BOOTCOUNT_AM33XX
/* End of undefs */

/* If we're compiling for the SPL, we don't have an env area */
#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_USBETH_SUPPORT)
#define CONFIG_ENV_IS_NOWHERE
#else
/* Otherwise, it's in an ext4 partition */
#ifdef CONFIG_CMD_EXT4
#define CONFIG_EXT4_WRITE
#define CONFIG_ENV_IS_IN_EXT4 1
#define CONFIG_ENV_SIZE 10 * 1024 /* Assume sector size of 1024 */
#define EXT4_ENV_INTERFACE "mmc"
#define EXT4_ENV_FILE "/uboot.env"
#define EXT4_ENV_DEVICE_AND_PART "1:3" /* U-boot environment location (dev:part) if _not_ using BOOTDEV_DETECT */
#endif
#endif /* CONFIG_SPL_BUILD */

/* If using boot device detection instead of hard coded device info */
#ifdef CONFIG_BOOTDEV_DETECT
#define BOOTDEV_DETECT_ENV_PART 3              /* U-boot environment partition if using BOOTDEV_DETECT */
#define BOOTDEV_DETECT_UPGRADE_PART 4          /* U-boot environment partition if using BOOTDEV_DETECT */
#define BOOTDEV_DETECT_ENV_PARAM "boot_detect" /* U-boot environment parameter used to store the detected boot device*/
#undef BOOTDEV_DETECT_ENV_SAVE                 /* Saves the environment back to disk after adding the detected boot device in PARAM */
#endif

/* If using Kubos system updates feature */
#ifdef CONFIG_UPDATE_KUBOS
#define CONFIG_SYS_DFU_DATA_BUF_SIZE 500 * SZ_1K /* File transfer chunk size */
#define CONFIG_SYS_DFU_MAX_FILE_SIZE 4 * SZ_1M   /* Maximum size for a single file.  Currently kernel (~2.5M) */
#define KUBOS_UPGRADE_DEVICE 0
#define KUBOS_UPGRADE_PART 1
#define KUBOS_UPGRADE_STORAGE CONFIG_SYS_LOAD_ADDR /* Temporary SDRAM storage location */

/* DFU Configuration */

/*
    If boot device detection is on, we want to be able to replace the target
    devices with the detected boot device.  So the string should contain
    fprintf style identifies everywhere the target device shows up.  We're
    leaving this as a default environment variable so it can be modified by
    user on a running system if desired.
*/
#ifdef CONFIG_BOOTDEV_DETECT
#define DFU_ALT_INFO_MMC         \
    "dfu_alt_info_mmc="          \
    "kernel fat %1d 1;"          \
    "rootfs part %1d 2;"         \
    "pumpkin-mbm2.dtb fat %1d 1" \
    "\0"
#else
#define DFU_ALT_INFO_MMC       \
    "dfu_alt_info_mmc="        \
    "kernel fat 1 1;"          \
    "rootfs part 1 2;"         \
    "uboot fat 1 1;"           \
    "pumpkin-mbm2.dtb fat 1 1" \
    "\0"
#endif
#define DFU_ALT_INFO_NOR ""
#else
#define DFU_ALT_INFO_MMC ""
#define DFU_ALT_INFO_NOR ""
#endif /* CONFIG_UPDATE_KUBOS */

#define CONFIG_BOOTCOMMAND \
    "run distro_bootcmd"

/*
    This defines several boot commands that use different devices for loading
    the linux rootfs.  It also makes an ordered list in which they are attempted
    by distro_bootcmd.  The order is top to bottom.  The standard LEGACY_MMC
    templates are defined in am335x_evm.h, but here we want to add a format
    that reads an environment parameter to uses it for device on which the
    rootfs resides. The new templates are named LEGACY_MMC_DEV and defined in
    the first two macros below.
*/
#define BOOTENV_DEV_LEGACY_MMC_ENV_PARAM(devtypeu, devtypel, envparam) \
    "bootcmd_" #devtypel "="                                           \
    "setenv mmcdev ${" #envparam "}; "                                 \
    "setenv bootpart ${" #envparam "}:2; "                             \
    "run mmcboot\0"

#define BOOTENV_DEV_NAME_LEGACY_MMC_ENV_PARAM(devtypeu, devtypel, envparam) \
    #devtypel " "

#ifdef CONFIG_BOOTDEV_DETECT
#define BOOT_TARGET_DEVICES(func)                                   \
    func(LEGACY_MMC_ENV_PARAM, legacy_mmc_boot_detect, boot_detect) \
        func(LEGACY_MMC_ENV_PARAM, legacy_mmc_boot_dev, boot_dev)   \
            func(LEGACY_MMC, legacy_mmc, 0)                         \
                func(LEGACY_MMC, legacy_mmc, 1)
#else
#define BOOT_TARGET_DEVICES(func)                        \
    func(LEGACY_MMC_ENV_PARAM, legacy_mmc_dev, boot_dev) \
        func(LEGACY_MMC, legacy_mmc, 0)                  \
            func(LEGACY_MMC, legacy_mmc, 1)
#endif

/* Make sure our full U-Boot build has a comprehensive environment */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_EXTRA_ENV_SETTINGS                                  \
    DEFAULT_LINUX_BOOT_ENV                                         \
    "args_mmc=run finduuid;setenv bootargs console=${console} "    \
    "${optargs} "                                                  \
    "root=/dev/mmcblk${mmcdev}p2 ro "                              \
    "rootfstype=${mmcrootfstype}\0"                                \
    "bootfile=kernel\0"                                            \
    "boot_dev=0\0"                                                 \
    "console=ttyS0,115200\0"                                       \
    "fdtfile=pumpkin-mbm2.dtb\0"                                   \
    "finduuid=part uuid mmc ${bootpart} uuid\0"                    \
    "loadimage=fatload mmc ${mmcdev}:1 ${loadaddr} /${bootfile}\0" \
    "loadfdt=fatload mmc ${mmcdev}:1 ${fdtaddr} /${fdtfile}\0"     \
    "mmcdev=0\0"                                                   \
    "mmcrootfstype=ext4 rootwait\0"                                \
    "mmcloados=run args_mmc; "                                     \
    "if run loadfdt; then "                                        \
    "bootm ${loadaddr} - ${fdtaddr}; "                             \
    "else "                                                        \
    "echo ERROR: Failed to load FTD file ${fdtfile}; "             \
    "fi;\0"                                                        \
    "mmcboot=mmc dev ${mmcdev}; "                                  \
    "if mmc rescan; then "                                         \
    "echo SD/MMC found on device ${mmcdev};"                       \
    "if run loadimage; then "                                      \
    "run mmcloados;"                                               \
    "fi;"                                                          \
    "fi;\0"                                                        \
    "optargs=\0" NETARGS                                           \
        BOOTENV                                                    \
            KUBOS_UPDATE_ARGS
#endif
