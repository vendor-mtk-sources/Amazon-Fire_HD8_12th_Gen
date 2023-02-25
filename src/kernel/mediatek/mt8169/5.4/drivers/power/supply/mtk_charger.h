/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include <linux/switch.h>

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

struct mtk_charger;
#define BATTERY_CV        4400000
#define BATTERY_CV_AGING1 4300000
#define BATTERY_CV_AGING2 4000000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
#define V_VBUS_UVLO 4300000 /* 4.3 V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define TEMP_T0_TO_T1_DEFAULT_CHARGING_CURRENT	350000 /* 350mA */
#define TEMP_T3_TO_T4_DEFAULT_CHARGING_CURRENT	2000000 /* 2000mA */

/* eoc protect */
#define DEFAULT_BAT_EOC_PROTECT_RESET_TIME	60
#define BAT_EOC_PROTECT_OFFSET_VOLTAGE	100
#define DEFAULT_BAT_SOC_EXIT_EOC		96	/* 96% */

#define WIRELESS_5W_CHARGER_CURRENT     1000000
#define WIRELESS_10W_CHARGER_CURRENT     1800000
#define WIRELESS_DEFAULT_CHARGER_CURRENT    500000

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

#define MAX_ALG_NO 10

#define BCM_FLAG_NONE              0
#define BCM_FLAG_CHARGE_TIMER      BIT(0)

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum vbus_voltage_state {
	VBUS_NORMAL = 0,
	VBUS_OVP,
	VBUS_UVLO
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4240000
#define JEITA_TEMP_T3_TO_T4_CV	4240000
#define JEITA_TEMP_T2_TO_T3_CV	4340000
#define JEITA_TEMP_T1_TO_T2_CV	4240000
#define JEITA_TEMP_T0_TO_T1_CV	4040000
#define JEITA_TEMP_BELOW_T0_CV	4040000
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES (-10)

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

enum adapter_power_category {
	ADAPTER_5W = 0,
	ADAPTER_7P5W,
	ADAPTER_9W,
	ADAPTER_12W,
	ADAPTER_15W,
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;
	int vbus_uvlo_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int non_std_ac_charger_current;
	int charging_host_charger_current;

	/*wireless*/
	int wireless_5w_charger_input_current;
	int wireless_5w_charger_current;
	int wireless_10w_charger_input_current;
	int wireless_10w_charger_current;
	int wireless_default_charger_input_current;
	int wireless_default_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;
	int temp_t0_to_t1_charger_current;
	int temp_t3_to_t4_charger_current;

	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

#ifdef CONFIG_MTK_USE_AGING_ZCV
	int battery_cv_aging1;	/* uv */
	int jeita_temp_above_t4_cv_aging1;
	int jeita_temp_t3_to_t4_cv_aging1;
	int jeita_temp_t2_to_t3_cv_aging1;
	int jeita_temp_t1_to_t2_cv_aging1;
	int jeita_temp_t0_to_t1_cv_aging1;
	int jeita_temp_below_t0_cv_aging1;

	int battery_cv_aging2;	/* uv */
	int jeita_temp_above_t4_cv_aging2;
	int jeita_temp_t3_to_t4_cv_aging2;
	int jeita_temp_t2_to_t3_cv_aging2;
	int jeita_temp_t1_to_t2_cv_aging2;
	int jeita_temp_t0_to_t1_cv_aging2;
	int jeita_temp_below_t0_cv_aging2;
#endif
};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int force_input_current_limit;
	int thermal_input_current_limit;
	int thermal_input_power_limit;
	int thermal_charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct power_detection_data {
	bool en;
	int iusb_ua;
	int type;
	int adapter_9w_aicl_min;
	int adapter_12w_aicl_min;
	int adapter_5w_iusb_lim;
	int adapter_7p5w_iusb_lim;
	int adapter_9w_iusb_lim;
	int adapter_12w_iusb_lim;
	int adapter_15w_iusb_lim;
	int aicl_trigger_iusb;
	int aicl_trigger_ichg;
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply  *chg_psy;
	struct power_supply  *bat_psy;

	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct mutex pd_lock;
	int pd_type;
	bool pd_reset;
	struct tcpc_device *tcpc;

	u32 bootmode;
	u32 boottype;

	unsigned int bcm_flag;

	int chr_type;
	int usb_state;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool disable_charger;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	bool vbusov_stat;
	bool is_chg_done;
	bool init_done;
	/* ATM */
	bool atm_enabled;

	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec charging_begin_time;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;

	bool enable_dynamic_mivr;

	/* Enable the feature detect bad charger */
	bool enable_bat_eoc_protect;
	bool bat_eoc_protect;
	uint32_t soc_exit_eoc;

	/* For INDPM mode workround */
	bool dpm_disable_charging;
	int soc_exit_dpm_eoc;
	int cv_enter_dpm_eoc;
	int dpm_state_count;

	int dpm_cv_bat_vdiff_uv;
	int dpm_ibat_threshold_ma;
	int dpm_state_count_max;
	int dpm_rechg_low_soc_diff;
	int dpm_input_cur_limit_ua;
	int dpm_charge_cur_limit_ua;

	/* For invalid charger det */
	bool invalid_charger_det_done;
	bool invalid_charger_det_weak;

	int invchg_check_iusb_ua;
	int invchg_check_ichg_ua;
	int invchg_check_mivr_uv;

	ktime_t disconnect_time;
	s64 disconnect_duration;
	s64 bat_eoc_protect_reset_time;

	/* top-off mode */
	unsigned int top_off_mode_enable;
	int custom_charging_cv;
	int top_off_mode_cv;

	unsigned long top_off_difference_full_cv;
	unsigned long normal_difference_full_cv;

	/* Adapter Power Detection */
	struct power_detection_data power_detection;

	/* For SW UVLO/OVP protection */
	int vbus_state;
	bool vbus_recovery_from_uvlo;

	struct switch_dev invalid_charger;

#ifdef CONFIG_MTK_USE_AGING_ZCV
	int top_off_mode_cv_aging1;
	int top_off_mode_cv_aging2;
#endif
};

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int set_battery_difference_full_cv(struct mtk_charger *info,
	unsigned long val);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);
extern int battery_get_soc(void);
extern void select_cv(struct mtk_charger *info);
extern int mtk_get_battery_cv(struct mtk_charger *info);

/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
extern bool is_mtk_charger_init_done(void);
#endif /* __MTK_CHARGER_H */
