/*
 * Copyright (C) 2019 Kubos Corporation
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

#include <common.h>
#include <net.h>
#include <ext4fs.h>
#include <fat.h>
#include <fs.h>
#include <mmc.h>
#include <kubos.h>
#include <dfu.h>

#ifdef CONFIG_BOOTDEV_DETECT
#include "../board/kubos/pumpkin-mbm2/bootdev_detect.h"
#endif

DECLARE_GLOBAL_DATA_PTR;

#define UPDATE_COUNT_ENVAR "kubos_updatecount"
#define DEV_ENVAR          "kubos_updatedev"
#define PART_ENVAR         "kubos_updatepart"
#define LOAD_ENVAR         "kubos_loadaddr"

int update_kubos_count(void)
{

    ulong count;
    int ret = 0;

    char *val = getenv(UPDATE_COUNT_ENVAR);

    if (val == NULL)
    {
        setenv(UPDATE_COUNT_ENVAR, "1");
    }
    else
    {
        count = simple_strtoul(val, NULL, 10);

        if (count > 1)
        {
            setenv(KUBOS_UPDATE_FILE, "bad");
            setenv(UPDATE_COUNT_ENVAR, "0");
            ret = -1;
        }
        else
        {
            count++;
            setenv_ulong(UPDATE_COUNT_ENVAR, count);
        }
    }

    saveenv();
    return ret;
}

int update_mmc(char * file, void * load_addr, char * dev_num)
{
    loff_t actlen;
    char * fit_image_name;
    int ndepth, noffset;
    // Get the file header
    int ret = ext4_read_file(file, load_addr, 0, sizeof(struct fdt_header), &actlen);

    if (ret < 0)
    {
        printf("ERROR: Couldn't read %s file - %d\n", file, ret);
        return -1;
    }

    // Get the total size of the FIT file (not including data sections)
    int fit_size = fdt_totalsize(load_addr);
    // Read the entire FIT file (not including data sections)
    ret = ext4_read_file(file, load_addr, 0, fit_size, &actlen);

    void * fit = load_addr;
    // The data section will be after the FIT file, but aligned on a 4-byte
    // boundary
    int data_offset = (fit_size + 3) & ~3;
    void * data_addr = load_addr + data_offset;

    // U-Boot reserves memory for itself at the end of RAM, while we're loading
    // the update file at the beginning.
    // So the largest update entity we could possibly allow is the amount of
    // space between the two, minus the room needed for the DFU chunk buffer,
    // minus a little bit of wiggle room (1K)
    int max_size = gd->start_addr_sp - (unsigned long) data_addr - CONFIG_SYS_DFU_DATA_BUF_SIZE - SZ_1K;

    // Validate the file
    if (!fit_check_format(fit)) {
        printf("ERROR: Bad FIT format of the update file, aborting "
        		"auto-update\n");
        return 1;
    }

    // Process updates
    noffset = fdt_path_offset(fit, FIT_IMAGES_PATH);
    ndepth = 0;

    while (noffset >= 0 && ndepth >= 0) {
        noffset = fdt_next_node(fit, noffset, &ndepth);

        if (ndepth != 1) {
            continue;
        }

        fit_image_name = (char *)fit_get_name(fit, noffset, NULL);
        printf("Processing update '%s' :", fit_image_name);

        // Get the update entity's size and offset
        int update_offset, update_size;
        if (fit_image_get_data_offset(fit, noffset, &update_offset)) {
            printf("Error: failed to get data offset, aborting\n");
            ret = 1;
            continue;
        }
        if (fit_image_get_data_size(fit, noffset, &update_size)) {
            printf("Error: failed to get data size, aborting\n");
            ret = 1;
            continue;
        }

        debug("Load addr: %p, Data offset: %d, Update offset: %d, Data size: %d\n",
                data_addr, data_offset, update_offset, update_size);

        if (update_size > max_size) {
        	printf("ERROR: Update file too large. %d > %d\n", update_size, max_size);
        	ret = 1;
        	continue;
        }

        // And load it into RAM
        ret = ext4_read_file(file, data_addr, data_offset + update_offset,
                update_size, &actlen);

        if (ret < 0)
        {
            printf("ERROR: Couldn't read data section - %d\n", ret);
            return -1;
        }

        if (!fit_image_verify_with_data(fit, noffset, data_addr, update_size)) {
            printf("ERROR: Invalid update hash, aborting\n");
            ret = 1;
            continue;
        }

        printf("\n");

        if (fit_image_check_type(fit, noffset,
                        IH_TYPE_FIRMWARE)) {
            ret = dfu_tftp_write(fit_image_name, (unsigned int) data_addr,
                         update_size, "mmc", dev_num);
            if (ret) {
                return ret;
            }
        } else {
        	printf("ERROR: Invalid image type\n");
        }
    }

    return 0;
}

/*
 * update_kubos
 *
 * U-Boot has a DFU utility which currently allows a board to download a new firmware package and
 * distribute the package components to the appropriate end-point locations.
 *
 * We want to leverage the second half of this utility and distribute a new firmware package that's
 * been copied into the upgrade partition, rather than via a USB/TFTP connection.
 *
 * Input:
 *    kubosUpdateType upgrade - Indicates whether we're installing a new package as part of an upgrade,
 *        or part of recovery.
 *
 * Returns:
 *    0 - An upgrade package was successfully installed
 *   -1 - No upgrade package could be installed (either because of system error or because no package exists)
 *   -2 - No upgrade package could be installed, but trigger a reboot so that we can retry.
 */

int update_kubos(bool upgrade)
{
    struct mmc *mmc;
    disk_partition_t part_info = {};
    char * file;
    char * env_addr;
    char * dfu_info;
    loff_t actlen;
    ulong addr, dev_num, part = 0;

    int ret = KUBOS_ERR_NO_REBOOT;

    /*
     * Get the name of the update file to load
     */
    file = getenv(KUBOS_UPDATE_FILE);
    if (file == NULL)
    {
        debug("INFO: %s envar not found\n", KUBOS_UPDATE_FILE);
        return KUBOS_ERR_NO_REBOOT;
    }
    else if (!strcmp(file, "none") || !strcmp(file, "bad"))
    {
        debug("INFO: No update file specified\n");
        return KUBOS_ERR_NO_REBOOT;
    }

    /*
     * Temp SDRAM address to load to
     */
    if ((env_addr = getenv(LOAD_ENVAR)) != NULL)
    {
        addr = simple_strtoul(env_addr, NULL, 16);
    }
    else
    {
        addr = KUBOS_UPGRADE_STORAGE;
    }

    /*
     * Get upgrade file device number
     */
#ifdef CONFIG_BOOTDEV_DETECT    
    dev_num = get_upgrade_device();
#else
    if ((env_addr = getenv(DEV_ENVAR)) != NULL)
    {
        dev_num = simple_strtoul(env_addr, NULL, 16);
    }
    else
    {
        dev_num = KUBOS_UPGRADE_DEVICE;
    }
#endif /* CONFIG_BOOTDEV_DETECT */

    /*
     * Load the device
     */
    mmc = find_mmc_device(dev_num);
    if (!mmc)
    {
        printf("ERROR: Could not access device: mmc%lu\n", dev_num);
        return KUBOS_ERR_NO_REBOOT;

    }

    ret = mmc_init(mmc);
    if (ret)
    {
        printf("ERROR: Could not init device - %d\n", ret);
        return KUBOS_ERR_NO_REBOOT;
    }

    /* Increase the upgrade attempt count */
    if (upgrade && update_kubos_count() != 0)
    {
        printf("ERROR: Number of update attempts exceeded. Abandoning update\n");
        return KUBOS_ERR_NO_REBOOT;
    }

    /*
     * Get and mount the upgrade file partition
     */
#ifdef CONFIG_BOOTDEV_DETECT    
    dev_num = get_upgrade_partition();
#else     
    if ((env_addr = getenv(PART_ENVAR)) != NULL)
    {
        part = simple_strtoul(env_addr, NULL, 16);
    }
    else
    {
        part = KUBOS_UPGRADE_PART;
    }
#endif /* CONFIG_BOOTDEV_DETECT */

    if (part_get_info(&mmc->block_dev, part, &part_info))
    {
        printf("ERROR: Could not mount upgrade partition.  No partition table\n");
        return KUBOS_ERR_NO_REBOOT;
    }

    debug("INFO: Checking for new firmware files\n");

    ext4fs_set_blk_dev(&mmc->block_dev, &part_info);

    ret = ext4fs_mount(0);
    if (!ret) {
        printf("ERROR: Could not mount upgrade partition. ext4fs mount err - %d\n", ret);
        return KUBOS_ERR_NO_REBOOT;
    }

    ret = ext4fs_exists(file);

    /*
     * Upgrade file found, call the existing DFU utility
     */
    if (ret)
    {
        debug("INFO: Found file to upgrade - %s\n", file);

        if (strstr(file,"nor") != NULL)
        {
            if ((dfu_info = getenv("dfu_alt_info_nor")) == NULL)
            {
                printf("ERROR: Can't upgrade nor files, dfu_alt_info_nor not defined\n");
                return KUBOS_ERR_REBOOT;
            }

            setenv("dfu_alt_info", dfu_info);

            ret = ext4_read_file(file, (void *)addr, 0, 0, &actlen);

            printf("ext4_read_file RC: %d. Size: %lld\n", ret, actlen);

            if (ret < 0)
            {
                printf("ERROR: Couldn't read %s file - %d\n", file, ret);
                return KUBOS_ERR_REBOOT;
            }
            else
            {
                /* The "0" parameter isn't used for NOR flash, but it has to be non-NULL */
                ret = update_tftp(addr, "nor", "0");
            }
        }
        else
        {
#ifdef CONFIG_BOOTDEV_DETECT
            if ((dfu_info = get_dfu_alt_info_mmc()) == NULL)      
#else        
            if ((dfu_info = getenv("dfu_alt_info_mmc")) == NULL)
#endif
            {
                printf("ERROR: Can't upgrade mmc files, dfu_alt_info_mmc not defined\n");
                return KUBOS_ERR_REBOOT;
            }

            setenv("dfu_alt_info", dfu_info);

            /*
             * For right now, the "0" parameter is useless. If you add a dfu_alt_info entity
             * in the future which has an entity type of "raw", this might need to change
             * to specify a non-zero mmc device number.
             */
            printf("Calling update_tftp\n");
            ret = update_mmc(file, (void *) addr, "0");
        }

        if (ret)
        {
            printf("ERROR: System upgrade failed - %d\n", ret);
            return KUBOS_ERR_REBOOT;
        }
        else
        {
            debug("INFO: Upgrade completed successfully\n");

            if (upgrade)
            {
                /*
                 * Only mark that we're using a new version of KubOS Linux if we're doing
                 * a regular upgrade (vs upgrading NOR flash files)
                 */
                if (strstr(file,"nor") == NULL)
                {
                    char *version = getenv(KUBOS_CURR_VERSION);
                    setenv(KUBOS_PREV_VERSION, version);
                    setenv(KUBOS_CURR_TRIED, "0");
                }
            }
            else
            {
                if (getenv_yesno(KUBOS_CURR_TRIED) == 1)
                {
                    setenv(KUBOS_PREV_VERSION, KUBOS_BASE);
                }
                else
                {
                    setenv(KUBOS_CURR_TRIED, "1");
                }
            }

            setenv(KUBOS_CURR_VERSION, file);
            bootcount_store(0);
        }
    }
    else
    {
        printf("ERROR: Upgrade file not found '%s'\n", file);
        return KUBOS_ERR_REBOOT;
    }

    /* Reset the updatefile name so that we resume usual boot after rebooting */
    setenv(KUBOS_UPDATE_FILE, "none");
    setenv(UPDATE_COUNT_ENVAR, "0");
    saveenv();

    return KUBOS_OK_REBOOT;
}

