/*
 * board.c
 *
 * Board functions for TI AM335X based boards
 *
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <spl.h>
#include <serial.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/omap.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/clock.h>
#include <asm/arch/clk_synthesizer.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mem.h>
#include <asm/io.h>
#include <asm/emif.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <miiphy.h>
#include <cpsw.h>
#include <power/tps65217.h>
#include <power/tps65910.h>
#include <environment.h>
#include <watchdog.h>
#include <environment.h>
#include "../common/board_detect.h"
#include "board.h"

#ifdef CONFIG_BOOTDEV_DETECT
#include "../../kubos/pumpkin-mbm2/bootdev_detect.h"
#endif


DECLARE_GLOBAL_DATA_PTR;

/* GPIO that controls power to DDR on EVM-SK */
#define GPIO_TO_PIN(bank, gpio)		(32 * (bank) + (gpio))
#define GPIO_DDR_VTT_EN		GPIO_TO_PIN(0, 7)
#define ICE_GPIO_DDR_VTT_EN	GPIO_TO_PIN(0, 18)
#define GPIO_PR1_MII_CTRL	GPIO_TO_PIN(3, 4)
#define GPIO_MUX_MII_CTRL	GPIO_TO_PIN(3, 10)
#define GPIO_FET_SWITCH_CTRL	GPIO_TO_PIN(0, 7)
#define GPIO_PHY_RESET		GPIO_TO_PIN(2, 5)

#if defined(CONFIG_SPL_BUILD) || \
	(defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_DM_ETH))
static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;
#endif

/*
 * Read header information from EEPROM into global structure.
 */
static inline int __maybe_unused read_eeprom(void)
{
	return ti_i2c_eeprom_am_get(-1, CONFIG_SYS_I2C_EEPROM_ADDR);
}

#ifndef CONFIG_DM_SERIAL
struct serial_device *default_serial_console(void)
{
	if (board_is_icev2())
		return &eserial4_device;
	else
		return &eserial1_device;
}
#endif

#ifndef CONFIG_SKIP_LOWLEVEL_INIT
static const struct ddr_data ddr2_data = {
	.datardsratio0 = MT47H128M16RT25E_RD_DQS,
	.datafwsratio0 = MT47H128M16RT25E_PHY_FIFO_WE,
	.datawrsratio0 = MT47H128M16RT25E_PHY_WR_DATA,
};

static const struct cmd_control ddr2_cmd_ctrl_data = {
	.cmd0csratio = MT47H128M16RT25E_RATIO,

	.cmd1csratio = MT47H128M16RT25E_RATIO,

	.cmd2csratio = MT47H128M16RT25E_RATIO,
};

static const struct emif_regs ddr2_emif_reg_data = {
	.sdram_config = MT47H128M16RT25E_EMIF_SDCFG,
	.ref_ctrl = MT47H128M16RT25E_EMIF_SDREF,
	.sdram_tim1 = MT47H128M16RT25E_EMIF_TIM1,
	.sdram_tim2 = MT47H128M16RT25E_EMIF_TIM2,
	.sdram_tim3 = MT47H128M16RT25E_EMIF_TIM3,
	.emif_ddr_phy_ctlr_1 = MT47H128M16RT25E_EMIF_READ_LATENCY,
};

static const struct ddr_data ddr3_data = {
	.datardsratio0 = MT41J128MJT125_RD_DQS,
	.datawdsratio0 = MT41J128MJT125_WR_DQS,
	.datafwsratio0 = MT41J128MJT125_PHY_FIFO_WE,
	.datawrsratio0 = MT41J128MJT125_PHY_WR_DATA,
};

static const struct ddr_data ddr3_beagleblack_data = {
	.datardsratio0 = MT41K256M16HA125E_RD_DQS,
	.datawdsratio0 = MT41K256M16HA125E_WR_DQS,
	.datafwsratio0 = MT41K256M16HA125E_PHY_FIFO_WE,
	.datawrsratio0 = MT41K256M16HA125E_PHY_WR_DATA,
};

static const struct ddr_data ddr3_evm_data = {
	.datardsratio0 = MT41J512M8RH125_RD_DQS,
	.datawdsratio0 = MT41J512M8RH125_WR_DQS,
	.datafwsratio0 = MT41J512M8RH125_PHY_FIFO_WE,
	.datawrsratio0 = MT41J512M8RH125_PHY_WR_DATA,
};

static const struct ddr_data ddr3_icev2_data = {
	.datardsratio0 = MT41J128MJT125_RD_DQS_400MHz,
	.datawdsratio0 = MT41J128MJT125_WR_DQS_400MHz,
	.datafwsratio0 = MT41J128MJT125_PHY_FIFO_WE_400MHz,
	.datawrsratio0 = MT41J128MJT125_PHY_WR_DATA_400MHz,
};

static const struct cmd_control ddr3_cmd_ctrl_data = {
	.cmd0csratio = MT41J128MJT125_RATIO,
	.cmd0iclkout = MT41J128MJT125_INVERT_CLKOUT,

	.cmd1csratio = MT41J128MJT125_RATIO,
	.cmd1iclkout = MT41J128MJT125_INVERT_CLKOUT,

	.cmd2csratio = MT41J128MJT125_RATIO,
	.cmd2iclkout = MT41J128MJT125_INVERT_CLKOUT,
};

static const struct cmd_control ddr3_beagleblack_cmd_ctrl_data = {
	.cmd0csratio = MT41K256M16HA125E_RATIO,
	.cmd0iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd1csratio = MT41K256M16HA125E_RATIO,
	.cmd1iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd2csratio = MT41K256M16HA125E_RATIO,
	.cmd2iclkout = MT41K256M16HA125E_INVERT_CLKOUT,
};

static const struct cmd_control ddr3_evm_cmd_ctrl_data = {
	.cmd0csratio = MT41J512M8RH125_RATIO,
	.cmd0iclkout = MT41J512M8RH125_INVERT_CLKOUT,

	.cmd1csratio = MT41J512M8RH125_RATIO,
	.cmd1iclkout = MT41J512M8RH125_INVERT_CLKOUT,

	.cmd2csratio = MT41J512M8RH125_RATIO,
	.cmd2iclkout = MT41J512M8RH125_INVERT_CLKOUT,
};

static const struct cmd_control ddr3_icev2_cmd_ctrl_data = {
	.cmd0csratio = MT41J128MJT125_RATIO_400MHz,
	.cmd0iclkout = MT41J128MJT125_INVERT_CLKOUT_400MHz,

	.cmd1csratio = MT41J128MJT125_RATIO_400MHz,
	.cmd1iclkout = MT41J128MJT125_INVERT_CLKOUT_400MHz,

	.cmd2csratio = MT41J128MJT125_RATIO_400MHz,
	.cmd2iclkout = MT41J128MJT125_INVERT_CLKOUT_400MHz,
};

static struct emif_regs ddr3_emif_reg_data = {
	.sdram_config = MT41J128MJT125_EMIF_SDCFG,
	.ref_ctrl = MT41J128MJT125_EMIF_SDREF,
	.sdram_tim1 = MT41J128MJT125_EMIF_TIM1,
	.sdram_tim2 = MT41J128MJT125_EMIF_TIM2,
	.sdram_tim3 = MT41J128MJT125_EMIF_TIM3,
	.zq_config = MT41J128MJT125_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41J128MJT125_EMIF_READ_LATENCY |
				PHY_EN_DYN_PWRDN,
};

static struct emif_regs ddr3_beagleblack_emif_reg_data = {
	.sdram_config = MT41K256M16HA125E_EMIF_SDCFG,
	.ref_ctrl = MT41K256M16HA125E_EMIF_SDREF,
	.sdram_tim1 = MT41K256M16HA125E_EMIF_TIM1,
	.sdram_tim2 = MT41K256M16HA125E_EMIF_TIM2,
	.sdram_tim3 = MT41K256M16HA125E_EMIF_TIM3,
	.zq_config = MT41K256M16HA125E_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41K256M16HA125E_EMIF_READ_LATENCY,
};

static struct emif_regs ddr3_evm_emif_reg_data = {
	.sdram_config = MT41J512M8RH125_EMIF_SDCFG,
	.ref_ctrl = MT41J512M8RH125_EMIF_SDREF,
	.sdram_tim1 = MT41J512M8RH125_EMIF_TIM1,
	.sdram_tim2 = MT41J512M8RH125_EMIF_TIM2,
	.sdram_tim3 = MT41J512M8RH125_EMIF_TIM3,
	.zq_config = MT41J512M8RH125_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41J512M8RH125_EMIF_READ_LATENCY |
				PHY_EN_DYN_PWRDN,
};

static struct emif_regs ddr3_icev2_emif_reg_data = {
	.sdram_config = MT41J128MJT125_EMIF_SDCFG_400MHz,
	.ref_ctrl = MT41J128MJT125_EMIF_SDREF_400MHz,
	.sdram_tim1 = MT41J128MJT125_EMIF_TIM1_400MHz,
	.sdram_tim2 = MT41J128MJT125_EMIF_TIM2_400MHz,
	.sdram_tim3 = MT41J128MJT125_EMIF_TIM3_400MHz,
	.zq_config = MT41J128MJT125_ZQ_CFG_400MHz,
	.emif_ddr_phy_ctlr_1 = MT41J128MJT125_EMIF_READ_LATENCY_400MHz |
				PHY_EN_DYN_PWRDN,
};

#ifdef CONFIG_SPL_OS_BOOT
int spl_start_uboot(void)
{
	/* break into full u-boot on 'c' */
	if (serial_tstc() && serial_getc() == 'c')
		return 1;

#ifdef CONFIG_SPL_ENV_SUPPORT
	env_init();
	env_relocate_spec();
	if (getenv_yesno("boot_os") != 1)
		return 1;
#endif

	return 0;
}
#endif

#define OSC	(V_OSCK/1000000)
const struct dpll_params dpll_ddr = {
		266, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_ddr_evm_sk = {
		303, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_ddr_bone_black = {
		400, OSC-1, 1, -1, -1, -1, -1};

void am33xx_spl_board_init(void)
{
	int mpu_vdd;

	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	/* Get the frequency */
	dpll_mpu_opp100.m = am335x_get_efuse_mpu_max_freq(cdev);

	if (board_is_bone() || board_is_bone_lt()) {
		/* BeagleBone PMIC Code */
		int usb_cur_lim;

		/*
		 * Only perform PMIC configurations if board rev > A1
		 * on Beaglebone White
		 */
		if (board_is_bone() && !strncmp(board_ti_get_rev(), "00A1", 4))
			return;

		if (i2c_probe(TPS65217_CHIP_PM))
			return;

		/*
		 * On Beaglebone White we need to ensure we have AC power
		 * before increasing the frequency.
		 */
		if (board_is_bone()) {
			uchar pmic_status_reg;
			if (tps65217_reg_read(TPS65217_STATUS,
					      &pmic_status_reg))
				return;
			if (!(pmic_status_reg & TPS65217_PWR_SRC_AC_BITMASK)) {
				puts("No AC power, disabling frequency switch\n");
				return;
			}
		}

		/*
		 * Override what we have detected since we know if we have
		 * a Beaglebone Black it supports 1GHz.
		 */
		if (board_is_bone_lt())
			dpll_mpu_opp100.m = MPUPLL_M_1000;

		/*
		 * Increase USB current limit to 1300mA or 1800mA and set
		 * the MPU voltage controller as needed.
		 */
		if (dpll_mpu_opp100.m == MPUPLL_M_1000) {
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1800MA;
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1325MV;
		} else {
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1300MA;
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1275MV;
		}

		if (tps65217_reg_write(TPS65217_PROT_LEVEL_NONE,
				       TPS65217_POWER_PATH,
				       usb_cur_lim,
				       TPS65217_USB_INPUT_CUR_LIMIT_MASK))
			puts("tps65217_reg_write failure\n");

		/* Set DCDC3 (CORE) voltage to 1.125V */
		if (tps65217_voltage_update(TPS65217_DEFDCDC3,
					    TPS65217_DCDC_VOLT_SEL_1125MV)) {
			puts("tps65217_voltage_update failure\n");
			return;
		}

		/* Set CORE Frequencies to OPP100 */
		do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);

		/* Set DCDC2 (MPU) voltage */
		if (tps65217_voltage_update(TPS65217_DEFDCDC2, mpu_vdd)) {
			puts("tps65217_voltage_update failure\n");
			return;
		}

		/*
		 * Set LDO3, LDO4 output voltage to 3.3V for Beaglebone.
		 * Set LDO3 to 1.8V and LDO4 to 3.3V for Beaglebone Black.
		 */
		if (board_is_bone()) {
			if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
					       TPS65217_DEFLS1,
					       TPS65217_LDO_VOLTAGE_OUT_3_3,
					       TPS65217_LDO_MASK))
				puts("tps65217_reg_write failure\n");
		} else {
			if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
					       TPS65217_DEFLS1,
					       TPS65217_LDO_VOLTAGE_OUT_1_8,
					       TPS65217_LDO_MASK))
				puts("tps65217_reg_write failure\n");
		}

		if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
				       TPS65217_DEFLS2,
				       TPS65217_LDO_VOLTAGE_OUT_3_3,
				       TPS65217_LDO_MASK))
			puts("tps65217_reg_write failure\n");
	} else {
		int sil_rev;

		/*
		 * The GP EVM, IDK and EVM SK use a TPS65910 PMIC.  For all
		 * MPU frequencies we support we use a CORE voltage of
		 * 1.1375V.  For MPU voltage we need to switch based on
		 * the frequency we are running at.
		 */
		if (i2c_probe(TPS65910_CTRL_I2C_ADDR))
			return;

		/*
		 * Depending on MPU clock and PG we will need a different
		 * VDD to drive at that speed.
		 */
		sil_rev = readl(&cdev->deviceid) >> 28;
		mpu_vdd = am335x_get_tps65910_mpu_vdd(sil_rev,
						      dpll_mpu_opp100.m);

		/* Tell the TPS65910 to use i2c */
		tps65910_set_i2c_control();

		/* First update MPU voltage. */
		if (tps65910_voltage_update(MPU, mpu_vdd))
			return;

		/* Second, update the CORE voltage. */
		if (tps65910_voltage_update(CORE, TPS65910_OP_REG_SEL_1_1_3))
			return;

		/* Set CORE Frequencies to OPP100 */
		do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);
	}

	/* Set MPU Frequency to what we detected now that voltages are set */
	do_setup_dpll(&dpll_mpu_regs, &dpll_mpu_opp100);
}

const struct dpll_params *get_dpll_ddr_params(void)
{
	enable_i2c0_pin_mux();
	i2c_init(CONFIG_SYS_OMAP24_I2C_SPEED, CONFIG_SYS_OMAP24_I2C_SLAVE);
	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	if (board_is_evm_sk())
		return &dpll_ddr_evm_sk;
	else if (board_is_bone_lt() || board_is_icev2())
		return &dpll_ddr_bone_black;
	else if (board_is_evm_15_or_later())
		return &dpll_ddr_evm_sk;
	else
		return &dpll_ddr;
}

void set_uart_mux_conf(void)
{
#if CONFIG_CONS_INDEX == 1
	enable_uart0_pin_mux();
#elif CONFIG_CONS_INDEX == 2
	enable_uart1_pin_mux();
#elif CONFIG_CONS_INDEX == 3
	enable_uart2_pin_mux();
#elif CONFIG_CONS_INDEX == 4
	enable_uart3_pin_mux();
#elif CONFIG_CONS_INDEX == 5
	enable_uart4_pin_mux();
#elif CONFIG_CONS_INDEX == 6
	enable_uart5_pin_mux();
#endif
}

void set_mux_conf_regs(void)
{
	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	enable_board_pin_mux();
}

const struct ctrl_ioregs ioregs_evmsk = {
	.cm0ioctl		= MT41J128MJT125_IOCTRL_VALUE,
	.cm1ioctl		= MT41J128MJT125_IOCTRL_VALUE,
	.cm2ioctl		= MT41J128MJT125_IOCTRL_VALUE,
	.dt0ioctl		= MT41J128MJT125_IOCTRL_VALUE,
	.dt1ioctl		= MT41J128MJT125_IOCTRL_VALUE,
};

const struct ctrl_ioregs ioregs_bonelt = {
	.cm0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm2ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
};

const struct ctrl_ioregs ioregs_evm15 = {
	.cm0ioctl		= MT41J512M8RH125_IOCTRL_VALUE,
	.cm1ioctl		= MT41J512M8RH125_IOCTRL_VALUE,
	.cm2ioctl		= MT41J512M8RH125_IOCTRL_VALUE,
	.dt0ioctl		= MT41J512M8RH125_IOCTRL_VALUE,
	.dt1ioctl		= MT41J512M8RH125_IOCTRL_VALUE,
};

const struct ctrl_ioregs ioregs = {
	.cm0ioctl		= MT47H128M16RT25E_IOCTRL_VALUE,
	.cm1ioctl		= MT47H128M16RT25E_IOCTRL_VALUE,
	.cm2ioctl		= MT47H128M16RT25E_IOCTRL_VALUE,
	.dt0ioctl		= MT47H128M16RT25E_IOCTRL_VALUE,
	.dt1ioctl		= MT47H128M16RT25E_IOCTRL_VALUE,
};

void sdram_init(void)
{
	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	if (board_is_evm_sk()) {
		/*
		 * EVM SK 1.2A and later use gpio0_7 to enable DDR3.
		 * This is safe enough to do on older revs.
		 */
		gpio_request(GPIO_DDR_VTT_EN, "ddr_vtt_en");
		gpio_direction_output(GPIO_DDR_VTT_EN, 1);
	}

	if (board_is_icev2()) {
		gpio_request(ICE_GPIO_DDR_VTT_EN, "ddr_vtt_en");
		gpio_direction_output(ICE_GPIO_DDR_VTT_EN, 1);
	}

	if (board_is_evm_sk())
		config_ddr(303, &ioregs_evmsk, &ddr3_data,
			   &ddr3_cmd_ctrl_data, &ddr3_emif_reg_data, 0);
	else if (board_is_bone_lt())
		config_ddr(400, &ioregs_bonelt,
			   &ddr3_beagleblack_data,
			   &ddr3_beagleblack_cmd_ctrl_data,
			   &ddr3_beagleblack_emif_reg_data, 0);
	else if (board_is_evm_15_or_later())
		config_ddr(303, &ioregs_evm15, &ddr3_evm_data,
			   &ddr3_evm_cmd_ctrl_data, &ddr3_evm_emif_reg_data, 0);
	else if (board_is_icev2())
		config_ddr(400, &ioregs_evmsk, &ddr3_icev2_data,
			   &ddr3_icev2_cmd_ctrl_data, &ddr3_icev2_emif_reg_data,
			   0);
	else
		config_ddr(266, &ioregs, &ddr2_data,
			   &ddr2_cmd_ctrl_data, &ddr2_emif_reg_data, 0);
}
#endif

#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))
static void request_and_set_gpio(int gpio, char *name)
{
	int ret;

	ret = gpio_request(gpio, name);
	if (ret < 0) {
		printf("%s: Unable to request %s\n", __func__, name);
		return;
	}

	ret = gpio_direction_output(gpio, 0);
	if (ret < 0) {
		printf("%s: Unable to set %s  as output\n", __func__, name);
		goto err_free_gpio;
	}

	gpio_set_value(gpio, 1);

	return;

err_free_gpio:
	gpio_free(gpio);
}

#define REQUEST_AND_SET_GPIO(N)	request_and_set_gpio(N, #N);

/**
 * RMII mode on ICEv2 board needs 50MHz clock. Given the clock
 * synthesizer With a capacitor of 18pF, and 25MHz input clock cycle
 * PLL1 gives an output of 100MHz. So, configuring the div2/3 as 2 to
 * give 50MHz output for Eth0 and 1.
 */
static struct clk_synth cdce913_data = {
	.id = 0x81,
	.capacitor = 0x90,
	.mux = 0x6d,
	.pdiv2 = 0x2,
	.pdiv3 = 0x2,
};
#endif

/*
 * Basic board specific setup.  Pinmux has been handled already.
 */
int board_init(void)
{

#ifdef CONFIG_BOOTDEV_DETECT
	save_omap_boot_params();
#endif

#if defined(CONFIG_HW_WATCHDOG)
	hw_watchdog_init();
#endif

	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
#if defined(CONFIG_NOR) || defined(CONFIG_NAND)
	gpmc_init();
#endif
#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD))
	int rv;

	if (board_is_icev2()) {
		REQUEST_AND_SET_GPIO(GPIO_PR1_MII_CTRL);
		REQUEST_AND_SET_GPIO(GPIO_MUX_MII_CTRL);
		REQUEST_AND_SET_GPIO(GPIO_FET_SWITCH_CTRL);
		REQUEST_AND_SET_GPIO(GPIO_PHY_RESET);

		rv = setup_clock_synthesizer(&cdce913_data);
		if (rv) {
			printf("Clock synthesizer setup failed %d\n", rv);
			return rv;
		}
	}
#endif

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	int rc;
	char *name = NULL;

	rc = read_eeprom();
	if (rc)
		puts("Could not get board ID.\n");

	if (board_is_bbg1())
		name = "BBG1";
	set_board_info_env(name);
#endif

#ifdef CONFIG_BOOTDEV_DETECT
    set_env_param_bootdev();
#endif

	return 0;
}
#endif

#ifndef CONFIG_DM_ETH

#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))
static void cpsw_control(int enabled)
{
	/* VTP can be added here */

	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr	= 0,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_addr	= 1,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};
#endif

#if ((defined(CONFIG_SPL_ETH_SUPPORT) || defined(CONFIG_SPL_USBETH_SUPPORT)) &&\
	defined(CONFIG_SPL_BUILD)) || \
	((defined(CONFIG_DRIVER_TI_CPSW) || \
	  defined(CONFIG_USB_ETHER) && defined(CONFIG_MUSB_GADGET)) && \
	 !defined(CONFIG_SPL_BUILD))

/*
 * This function will:
 * Read the eFuse for MAC addresses, and set ethaddr/eth1addr/usbnet_devaddr
 * in the environment
 * Perform fixups to the PHY present on certain boards.  We only need this
 * function in:
 * - SPL with either CPSW or USB ethernet support
 * - Full U-Boot, with either CPSW or USB ethernet
 * Build in only these cases to avoid warnings about unused variables
 * when we build an SPL that has neither option but full U-Boot will.
 */
int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;
	__maybe_unused struct ti_am_eeprom *header;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))
	if (!getenv("ethaddr")) {
		printf("<ethaddr> not set. Validating first E-fuse MAC\n");

		if (is_valid_ethaddr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
	}

#ifdef CONFIG_DRIVER_TI_CPSW

	mac_lo = readl(&cdev->macid1l);
	mac_hi = readl(&cdev->macid1h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("eth1addr")) {
		if (is_valid_ethaddr(mac_addr))
			eth_setenv_enetaddr("eth1addr", mac_addr);
	}

	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	if (board_is_bone() || board_is_bone_lt() ||
	    board_is_idk()) {
		writel(MII_MODE_ENABLE, &cdev->miisel);
		cpsw_slaves[0].phy_if = cpsw_slaves[1].phy_if =
				PHY_INTERFACE_MODE_MII;
	} else if (board_is_icev2()) {
		writel(RMII_MODE_ENABLE | RMII_CHIPCKL_ENABLE, &cdev->miisel);
		cpsw_slaves[0].phy_if = PHY_INTERFACE_MODE_RMII;
		cpsw_slaves[1].phy_if = PHY_INTERFACE_MODE_RMII;
		cpsw_slaves[0].phy_addr = 1;
		cpsw_slaves[1].phy_addr = 3;
	} else {
		writel((RGMII_MODE_ENABLE | RGMII_INT_DELAY), &cdev->miisel);
		cpsw_slaves[0].phy_if = cpsw_slaves[1].phy_if =
				PHY_INTERFACE_MODE_RGMII;
	}

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;
#endif

	/*
	 *
	 * CPSW RGMII Internal Delay Mode is not supported in all PVT
	 * operating points.  So we must set the TX clock delay feature
	 * in the AR8051 PHY.  Since we only support a single ethernet
	 * device in U-Boot, we only do this for the first instance.
	 */
#define AR8051_PHY_DEBUG_ADDR_REG	0x1d
#define AR8051_PHY_DEBUG_DATA_REG	0x1e
#define AR8051_DEBUG_RGMII_CLK_DLY_REG	0x5
#define AR8051_RGMII_TX_CLK_DLY		0x100

	if (board_is_evm_sk() || board_is_gp_evm()) {
		const char *devname;
		devname = miiphy_get_current_dev();

		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_ADDR_REG,
				AR8051_DEBUG_RGMII_CLK_DLY_REG);
		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_DATA_REG,
				AR8051_RGMII_TX_CLK_DLY);
	}
#endif
#if defined(CONFIG_USB_ETHER) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USBETH_SUPPORT))
	if (is_valid_ethaddr(mac_addr))
		eth_setenv_enetaddr("usbnet_devaddr", mac_addr);

	rv = usb_eth_initialize(bis);
	if (rv < 0)
		printf("Error %d registering USB_ETHER\n", rv);
	else
		n += rv;
#endif
	return n;
}
#endif

#endif /* CONFIG_DM_ETH */

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	if (board_is_gp_evm() && !strcmp(name, "am335x-evm"))
		return 0;
	else if (board_is_bone() && !strcmp(name, "am335x-bone"))
		return 0;
	else if (board_is_bone_lt() && !strcmp(name, "am335x-boneblack"))
		return 0;
	else if (board_is_evm_sk() && !strcmp(name, "am335x-evmsk"))
		return 0;
	else if (board_is_bbg1() && !strcmp(name, "am335x-bonegreen"))
		return 0;
	else if (board_is_icev2() && !strcmp(name, "am335x-icev2"))
		return 0;
	else
		return -1;
}
#endif
