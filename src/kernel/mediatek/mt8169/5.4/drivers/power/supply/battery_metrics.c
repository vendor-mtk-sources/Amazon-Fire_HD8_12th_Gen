#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/metricslog.h>
#include <linux/power_supply.h>

#include "mtk_battery.h"
#include "mtk_charger.h"
#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
#include <linux/amzn_sign_of_life.h>
#endif
#if IS_ENABLED(CONFIG_DRM)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define BATTERY_METRICS_BUFF_SIZE 512
char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE];

enum {
	SCREEN_OFF,
	SCREEN_ON,
};

#define bat_metrics_log(domain, fmt, ...)				\
do {									\
	memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);		\
	snprintf(g_metrics_buf, sizeof(g_metrics_buf),	fmt, ##__VA_ARGS__);\
	log_to_metrics(ANDROID_LOG_INFO, domain, g_metrics_buf);	\
} while (0)

struct screen_state {
	struct timespec64 screen_on_time;
	struct timespec64 screen_off_time;
	int screen_on_soc;
	int screen_off_soc;
	int screen_state;
};

struct pm_state {
	struct timespec64 suspend_ts;
	struct timespec64 resume_ts;
	int suspend_soc;
	int resume_soc;
	int suspend_bat_car;
	int resume_bat_car;
};

struct iavg_data {
	struct timespec64 last_ts;
	int pre_bat_car;
	int pre_screen_state;
};

struct bat_metrics_data {
	bool is_top_off_mode;
	bool metrics_psy_ready;
	u8 fault_type_old;
	u32 chg_sts_old;
	unsigned int bcm_flag_old;

	struct mtk_gauge *gauge;
	struct mtk_battery *gm;
	struct iavg_data iavg;
	struct screen_state screen;
	struct pm_state pm;
	struct delayed_work dwork;
#if IS_ENABLED(CONFIG_DRM)
	struct notifier_block pm_notifier;
#endif
};
static struct bat_metrics_data metrics_data;

static int metrics_get_property(enum gauge_property gp, int *val)
{
	struct mtk_gauge_sysfs_field_info *attr;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	attr = metrics_data.gauge->attr;
	if (attr == NULL) {
		pr_err("%s attr = NULL\n", __func__);
		return -ENODEV;
	}

	if (attr[gp].prop == gp) {
		mutex_lock(&metrics_data.gauge->ops_lock);
		attr[gp].get(metrics_data.gauge, &attr[gp], val);
		mutex_unlock(&metrics_data.gauge->ops_lock);
	} else {
		pr_err("%s attr = NULL\n", __func__);
		return -ENOTSUPP;
	}

	return 0;
}

static int metrics_get_int_property(enum gauge_property gp)
{
	int val, ret;

	ret = metrics_get_property(gp, &val);
	if (ret != 0) {
		pr_err("%s val = NULL\n", __func__);
		return 0;
	}

	return val;
}

int bat_metrics_adapter_power(u32 type, u32 aicl_ma)
{
	static const char * const category_text[] = {
		"5W", "7.5W", "9W", "12W", "15W"
	};

	if (type >= sizeof(category_text))
		return 0;

	bat_metrics_log("battery",
			"adapter_power:def:adapter_%s=1;CT;1,aicl=%d;CT;1:NR",
			category_text[type], (aicl_ma < 0) ? 0 : aicl_ma);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_adapter_power);

int bat_metrics_chg_fault(u8 fault_type)
{
	static const char * const charger_fault_text[] = {
		"NONE", "VBUS_OVP", "VBAT_OVP", "SAFETY_TIMEOUT"
	};

	if (metrics_data.fault_type_old == fault_type)
		return 0;

	metrics_data.fault_type_old = fault_type;
	if (fault_type != 0)
		bat_metrics_log("battery",
			"charger:def:charger_fault_type_%s=1;CT;1:NA",
			charger_fault_text[fault_type]);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_chg_fault);

int bat_metrics_bcm_flag(unsigned int bcm_flag)
{
	if (metrics_data.bcm_flag_old == bcm_flag)
		return 0;

	metrics_data.bcm_flag_old = bcm_flag;

	switch (bcm_flag) {
	case BCM_FLAG_CHARGE_TIMER:
		bat_metrics_log("battery", "battery:def:bcm_flag_%s=1;CT;1:NA",
			 "charge_timer");
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_bcm_flag);

int bat_metrics_chrdet(u32 chr_type)
{
	static const char * const charger_type_text[] = {
		"UNKNOWN", "BATTERY", "UPS", "MAINS", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_TYPE_C",
		"USB_PD", "USB_PD_DRP", "APPLE_BRICK_ID", "WIRELESS", "USB_HVDCP", "USB_HVDCP_3",
		"USB_HVDCP_3P5", "USB_FLOAT", "WIRELESS_5W", "WIRELESS_10W"
	};

	if (chr_type > POWER_SUPPLY_TYPE_UNKNOWN &&
		chr_type <= POWER_SUPPLY_TYPE_WIRELESS_10W) {
		bat_metrics_log("USBCableEvent",
			"%s:charger:chg_type_%s=1;CT;1:NR",
			__func__, charger_type_text[chr_type]);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_chrdet);

static int metrics_get_average_current(bool *valid)
{
	int iavg = 0;
	int ver;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	ver = metrics_get_int_property(GAUGE_PROP_HW_VERSION);

	if (metrics_data.gm->disableGM30) {
		iavg = 0;
	} else {
		if (ver >= GAUGE_HW_V1000 &&
			ver < GAUGE_HW_V2000) {
			iavg = metrics_data.gm->sw_iavg;
		} else {
			*valid = metrics_data.gm->gauge->fg_hw_info.current_avg_valid;
			iavg =
			metrics_get_int_property(GAUGE_PROP_AVERAGE_CURRENT);
		}
	}

	return iavg;
}

int bat_metrics_chg_state(u32 chg_sts)
{
	int vbat, ibat_avg;
	bool valid = 0;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	if (metrics_data.chg_sts_old == chg_sts)
		return 0;

	vbat = metrics_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE);
	ibat_avg = metrics_get_average_current(&valid);
	metrics_data.chg_sts_old = chg_sts;
	bat_metrics_log("battery",
		"charger:def:POWER_STATUS_%s=1;CT;1,cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
		(chg_sts == POWER_SUPPLY_STATUS_CHARGING) ?
		"CHARGING" : "DISCHARGING", metrics_data.gm->soc, vbat, ibat_avg);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_chg_state);

int bat_metrics_critical_shutdown(void)
{
	bat_metrics_log("battery", "battery:def:critical_shutdown=1;CT;1:HI");
#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
	life_cycle_set_special_mode(LIFE_CYCLE_SMODE_LOW_BATTERY);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_critical_shutdown);

int bat_metrics_top_off_mode(bool is_on)
{
	int vbat;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	if (metrics_data.is_top_off_mode == is_on)
		return 0;

	vbat = metrics_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE);
	metrics_data.is_top_off_mode = is_on;
	if (is_on) {
		bat_metrics_log("battery",
		"battery:def:Charging_Over_7days=1;CT;1,Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1:NA",
		vbat, metrics_data.gm->ui_soc, metrics_data.gm->soc,
		metrics_data.gm->tbat_precise);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_top_off_mode);

int bat_metrics_aging(int aging_factor, int bat_cycle, int qmax)
{
	static struct timespec64 last_log_time;
	struct timespec64 now_time, diff;

	ktime_get_boottime_ts64(&now_time);
	diff = timespec64_sub(now_time, last_log_time);

	if (last_log_time.tv_sec != 0 && diff.tv_sec < 3600)
		return 0;

	pr_info("[%s]diff time:%ld\n", __func__, diff.tv_sec);
	bat_metrics_log("battery",
		"battery:def:aging_factor=%d;CT;1,bat_cycle=%d;CT;1,qmax=%d;CT;1:NA",
		aging_factor / 100, bat_cycle, qmax / 10);
	ktime_get_boottime_ts64(&last_log_time);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_aging);

int bat_metrics_invalid_charger(void)
{
	bat_metrics_log("battery", "battery:def:invalid_charger=1;CT;1:NA");

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_invalid_charger);

int bat_metrics_dpm_eoc_mode(int vcharge)
{
	bat_metrics_log("battery",
		"battery:def:dpm_eoc=1;CT;1,Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Vcharge=%d;CT;1:NA",
		metrics_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE),
		metrics_data.gm->ui_soc, metrics_data.gm->soc, vcharge / 1000);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_dpm_eoc_mode);

#if IS_ENABLED(CONFIG_DRM)
int bat_metrics_screen_on(void)
{
	struct timespec64 screen_on_time;
	struct timespec64 diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc, diff_soc;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	screen->screen_state = SCREEN_ON;
	soc = metrics_data.gm->soc;
	ktime_get_boottime_ts64(&screen_on_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_off_soc - soc;
	diff = timespec64_sub(screen_on_time, screen->screen_off_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
	pr_info("%s: diff_soc: %d[%d -> %d] elapsed=%ld\n", __func__,
		diff_soc, screen->screen_off_soc, soc, elaps_msec);
	bat_metrics_log("drain_metrics",
		"screen_off_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);

exit:
	screen->screen_on_time = screen_on_time;
	screen->screen_on_soc = soc;
	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_screen_on);

int bat_metrics_screen_off(void)
{
	struct timespec64 screen_off_time;
	struct timespec64 diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc, diff_soc;

	if (!metrics_data.metrics_psy_ready)
		return -ENODEV;

	screen->screen_state = SCREEN_OFF;
	soc = metrics_data.gm->soc;
	ktime_get_boottime_ts64(&screen_off_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_on_soc - soc;
	diff = timespec64_sub(screen_off_time, screen->screen_on_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
	pr_info("%s: diff_soc: %d[%d -> %d] elapsed=%ld\n", __func__,
		diff_soc, screen->screen_on_soc, soc, elaps_msec);
	bat_metrics_log("drain_metrics",
		"screen_on_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);

exit:
	screen->screen_off_time = screen_off_time;
	screen->screen_off_soc = soc;
	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_screen_off);

static int pm_notifier_callback(struct notifier_block *notify,
			unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK)
			bat_metrics_screen_on();
		else if (*blank == FB_BLANK_POWERDOWN)
			bat_metrics_screen_off();
	}

	return 0;
}
#endif

#define SUSPEND_RESUME_INTEVAL_MIN 30
int bat_metrics_suspend(void)
{
	struct pm_state *pm = &metrics_data.pm;

	ktime_get_boottime_ts64(&pm->suspend_ts);
	pm->suspend_bat_car = metrics_get_int_property(GAUGE_PROP_COULOMB);

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_suspend);

int bat_metrics_resume(void)
{
	struct pm_state *pm = &metrics_data.pm;
	struct timespec64 resume_ts;
	struct timespec64 sleep_ts;
	int diff_bat_car, ibat_avg;
	int resume_bat_car = -1;

	ktime_get_boottime_ts64(&resume_ts);
	sleep_ts = timespec64_sub(resume_ts, pm->suspend_ts);
	if (sleep_ts.tv_sec < SUSPEND_RESUME_INTEVAL_MIN)
		goto exit;

	resume_bat_car = metrics_get_int_property(GAUGE_PROP_COULOMB);
	diff_bat_car = resume_bat_car - pm->suspend_bat_car;
	ibat_avg = diff_bat_car * 3600 / sleep_ts.tv_sec / 10;
	pr_info("IBAT_AVG: sleep: diff_car[%d %d]=%d,time=%ld,ibat_avg=%d\n",
			pm->suspend_bat_car, resume_bat_car,
			diff_bat_car, sleep_ts.tv_sec, ibat_avg);

exit:
	pm->resume_bat_car = resume_bat_car;
	pm->resume_ts = resume_ts;
	pm->resume_bat_car = resume_bat_car;
	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_resume);

static void bat_metrics_work(struct work_struct *work)
{
	struct bat_metrics_data *data =
		container_of(work, struct bat_metrics_data, dwork.work);
	struct iavg_data *iavg = &data->iavg;
	struct timespec64 ts_now, diff_ts;

	int bat_car, diff_bat_car, ibat_now, ibat_avg, sign;
	int screen_state = metrics_data.screen.screen_state;

	ktime_get_boottime_ts64(&ts_now);
	bat_car = metrics_get_int_property(GAUGE_PROP_COULOMB);
	if (iavg->pre_bat_car == -1)
		goto again;

	if (screen_state != iavg->pre_screen_state) {
		pr_info("%s: screen state change, drop data\n", __func__);
		goto again;
	}

	diff_ts = timespec64_sub(ts_now, iavg->last_ts);
	ibat_now = metrics_get_int_property(GAUGE_PROP_BATTERY_CURRENT); /* mA */
	if (bat_car > iavg->pre_bat_car)
		sign = 1;
	else
		sign = -1;
	diff_bat_car = abs(iavg->pre_bat_car - bat_car); /* 0.1mAh */
	ibat_avg = sign * diff_bat_car * 3600 / diff_ts.tv_sec / 10; /* mA */

	pr_info("IBAT_AVG: %s: diff_car[%d %d]=%d,time=%ld,ibat=%d,ibat_avg=%d\n",
			screen_state == SCREEN_ON ? "screen_on" : "screen_off",
			iavg->pre_bat_car, bat_car, diff_bat_car,
			diff_ts.tv_sec, ibat_now, ibat_avg);

again:
	iavg->pre_bat_car = bat_car;
	iavg->last_ts = ts_now;
	iavg->pre_screen_state = screen_state;
	queue_delayed_work(system_freezable_wq, &data->dwork, 10 * HZ);
}

int bat_metrics_init(void)
{
	int ret = 0;
	struct power_supply *gauge_psy;
	struct power_supply *battery_psy;

	metrics_data.screen.screen_on_soc = -1;
	metrics_data.screen.screen_off_soc = -1;
	metrics_data.pm.suspend_soc = -1;
	metrics_data.pm.resume_soc = -1;
	metrics_data.pm.suspend_bat_car = -1;
	metrics_data.pm.resume_bat_car = -1;
	metrics_data.iavg.pre_bat_car = -1;

	gauge_psy = power_supply_get_by_name("mtk-gauge");
	if (gauge_psy == NULL)
		return -ENODEV;

	battery_psy = power_supply_get_by_name("battery");
	if (battery_psy == NULL)
		return -ENODEV;

	metrics_data.gauge = (struct mtk_gauge *)power_supply_get_drvdata(gauge_psy);
	metrics_data.gm = (struct mtk_battery *)power_supply_get_drvdata(battery_psy);
	metrics_data.metrics_psy_ready = TRUE;

	INIT_DELAYED_WORK(&metrics_data.dwork, bat_metrics_work);
	queue_delayed_work(system_freezable_wq, &metrics_data.dwork, 0);
#if IS_ENABLED(CONFIG_DRM)
	metrics_data.pm_notifier.notifier_call = pm_notifier_callback;
	ret = fb_register_client(&metrics_data.pm_notifier);
	if (ret)
		pr_err("%s: fail to register pm notifier\n", __func__);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(bat_metrics_init);

void bat_metrics_uninit(void)
{
	metrics_data.metrics_psy_ready = FALSE;
#if IS_ENABLED(CONFIG_DRM)
	fb_unregister_client(&metrics_data.pm_notifier);
#endif
}
EXPORT_SYMBOL_GPL(bat_metrics_uninit);

MODULE_DESCRIPTION("Amazon battery metrics");
MODULE_LICENSE("GPL v2");
