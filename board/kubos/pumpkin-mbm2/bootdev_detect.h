
#ifndef __BOOTDEV_DETECT_H
#define __BOOTDEV_DETECT_H

#include <common.h>
#include <spl.h>

DECLARE_GLOBAL_DATA_PTR;

/*
    ------------------------------------------------------------------------
    The AM335x processor has a register entry with an identifier for the
    boot device used at the last power-on boot (see spl.h for deivice
    definitions).  This is the device that will be used to search for the
    MBR and boot partition (partition 1) on all subsequent resets until
    there is a new full power-on boot.  The functions in this file, along
    with some configuration settings in pumpkin-mbm2.h, are intended to use
    this regiser value to override hard-coded device and partition settings
    for loading the U-Boot environment and the Kubos upgrade feature.  This
    is useful for systems that want to have fully independent and redundant
    boot disks on the internal eMMC and micro-SD card that can be selected
    from an external GPIO signal using the SYS_BOOT2 line, which is exposed
    on the BeagleBone Black.
    ------------------------------------------------------------------------
*/

/*
    Returns the power-on boot device number (if it was an MMC device).  Note
    that the mmc device number is one less than the defined flag names.
*/
static inline int get_boot_mmc_device(void)
{
    switch (gd->arch.omap_boot_device)
    {
    case BOOT_DEVICE_MMC1:
        return 0;
    case BOOT_DEVICE_MMC2:
        return 1;
    }
    return -1;
}

/*
    Provides a hook if you want to do custom logic to determine the mmc
    device used for loading the U-Boot environment.  For now, we assume the
    environment file is on the same device as used by the system boot.
*/
static inline int get_env_device(void)
{
    return get_boot_mmc_device();
}

/*
    Provides a hook if you want to do custom logic to determine the
    partition used for loading the U-Boot environment file.  For now, we
    assume the environment file is in the same partition on either mmc
    device and return the defined value from pumpkin-mbm2.h.
*/
static inline int get_env_partition(void)
{
    return BOOTDEV_DETECT_ENV_PART;
}

/* Similar to get_env_partition() above */
static inline int get_boot_upgrade_partition(void)
{
    return BOOTDEV_DETECT_UPGRADE_PART;
}

/*
    Convenience function to return a short string containing "dev:part"
    (e.g. "1:3") for the environment location.
*/
static inline char *get_env_devpart(void)
{
    static char devpart[16];
    snprintf(devpart, 16, "%d:%d", get_env_device(), get_env_partition());
    return devpart;
}

/* Stores the detected boot device in a predefined environment parameter.
   Optionally saves the environment to disk so this information is available
   within in the booted linux.
*/
static inline void set_env_param_bootdev(void)
{
    char devstr[8];
    snprintf(devstr, 8, "%d", get_env_device());

    printf("Setting %s environment variable to %s\n",
           BOOTDEV_DETECT_ENV_PARAM, devstr);
    setenv(BOOTDEV_DETECT_ENV_PARAM, devstr);

#ifdef BOOTDEV_DETECT_ENV_SAVE
    printf("Saving environment\n");
    saveenv();
#endif
}

/*
    ------------------------------------------------------------------------
    If the Kubos Upgrade feature is enabled, add functions to return the
    device and partition where the upgrade image (.itb) resides.
    ------------------------------------------------------------------------
*/
#ifdef CONFIG_UPDATE_KUBOS

/* Similar to get_env_device() above */
static inline int get_upgrade_device(void)
{
    return get_boot_mmc_device();
}

/* Similar to get_env_partition() above */
static inline int get_upgrade_partition(void)
{
    return KUBOS_UPGRADE_PART;
}

/*
    Returns a string containing the dfu_alt_info_mmc information that
    specifies the images to load from the upgrade .itb file and the
    destinations.  Any instance of "%1$d" in the dfu_alt_info_mmc string
    is replaced with the upgrade device number.
*/
static inline char *get_dfu_alt_info_mmc(void)
{
    static char dfu_info[256];
    int upgrade_dev = get_upgrade_device();

    /*
     * We need three copies of upgrade_dev because U-Boot does not support the %1$
     * syntax to "lock" everything to the first variable and we have 3 %1d's in our dfu_string
     */
    snprintf(dfu_info, 256, getenv("dfu_alt_info_mmc"), upgrade_dev, upgrade_dev, upgrade_dev);

    return dfu_info;
}

#endif /* CONFIG_UPDATE_KUBOS */

#endif /* __BOOTDEV_DETECT_H */
