 /************************************************************
 *
 * file: p922x_wireless_power.h
 *
 * Description: Interface of P9221 to AP access included file
 *
 *------------------------------------------------------------
 *
 * Copyright (c) 2021, Integrated Device Technology Co., Ltd.
 * Copyright (C) 2021 Amazon.com Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *************************************************************/

#ifndef __IDTP9220_H__
#define __IDTP9220_H__

/* RX -> TX */
#define PROPRIETARY18		0x18
#define PROPRIETARY28		0x28
#define PROPRIETARY38		0x38
#define PROPRIETARY48		0x48
#define PROPRIETARY58		0x58

/* bits mask */
#define BIT0			0x01
#define BIT1			0x02
#define BIT2			0x04
#define BIT3			0x08
#define BIT4			0x10
#define BIT5			0x20
#define BIT6			0x40
#define BIT7			0x80
#define BIT8			(1 << 8)
#define BIT9			(1 << 9)
#define BIT12			(1 << 12)
#define BIT13			(1 << 13)

/* status low regiter bits define */
#define STATUS_VOUT_ON		BIT7
#define STATUS_VOUT_OFF		BIT6
#define STATUS_OV_TEMP		BIT2
#define STATUS_OV_VOL		BIT1
#define STATUS_OV_CURR		BIT0

/*
 * bitmap for status flags
 * 1: indicates a pending interrupt for LDO Vout state change from OFF to ON
 */
#define VOUTCHANGED		BIT7 /* Stat_Vout_ON */
/*
 * 1: indicates a pending interrupt for TX Data Received.
 * (Change from "No Received Data" state to "data Received" state)
 */
#define TXDATARCVD		BIT4 /* TX Data Received */

/* used registers define */
#define REG_CHIP_ID		0x5870
#define REG_ADC_NTC		0x0010
#define REG_CHIP_REV		0x001c
#define REG_PRX			0x0026
#define REG_STATUS		0x0034
#define REG_INTR		0x0036
#define REG_INTR_EN		0x0038
#define REG_CHG_STATUS		0x003A
#define REG_EPT			0X003B
#define REG_ADC_VOUT		0x003C
#define REG_VOUT_SET		0x003E
#define REG_VRECT_ADJ		0x003F
#define REG_ADC_VRECT		0x0040
#define REG_RX_LOUT		0x0044
#define REG_ADC_TEMP		0x0046
#define REG_ILIM_SET		0x004A
/* Signal Strength Register */
#define REG_SS			0x004B
#define REG_COMMAND		0x004E
/* Proprietary Packet Header Register, PPP_Header (0x50) */
#define REG_PROPPKT_ADDR	0x0050
/* PPP Data Value Register(0X51, 0x52, 0x53, 0x54, 0x55) */
#define REG_PPPDATA_ADDR	0x0051
#define REG_INT_CLEAR		0x0056
/* Back Channel Packet Register (0x58) */
#define REG_BCHEADER_ADDR	0x0058
/* Back Channel Packet Register (0x59, 0x5A, 0x5B, 0x5C) */
#define REG_BCDATA_ADDR		0x005A
/* Communocation CAP Enable */
#define REG_CM_CAP_EN_ADDR	0x0063
/* FOD parameters addr, 16 bytes */
#define REG_FOD_COEF_ADDR	0x0068
#define REG_FOD_DUMP_MAX	0x0077
#define REG_FC_VOLTAGE		0x0078
#define REG_IOUT_RAW		0x007A
#define REG_DUMP_MAX		0x007B

#define IDT_FAST_CHARING_EN	0
#define IDT_PROGRAM_FIRMWARE_EN	1

#define CHARGER_VOUT_10W 9000
#define CHARGER_VOUT_5W 6500

#define READY_DETECT_TIME (50*HZ/1000)

#define SWITCH_DETECT_TIME (300*HZ/1000)
#define SWITCH_10W_VTH_L 8500
#define SWITCH_10W_VTH_H 9500
#define SWITCH_5W_VTH_L 6000
#define SWITCH_5W_VTH_H 7000
#define SWITCH_VOLTAGE_COUNT 6
#define DIS_CM_CAP	0x00
#define EN_CM_CAP	0X30

/* bitmap for SSCmnd register 0x4e */
#define VSWITCH			BIT7
/*
 * If AP sets this bit to "1" then IDTP9220 M0 clears the interrupt
 * corresponding to the bit(s) which has a value of "1"
 */
#define CLRINT			BIT5

/*
 * If AP sets this bit to 1 then IDTP9220 M0 sends the Charge Status packet
 * (defined in the Battery Charge Status Register) to TX and then sets this bit to 0
 */
#define CHARGE_STAT		BIT4

/*
 * If AP sets this bit to "1" then IDTP9220 M0 sends the End of Power packet
 * (define in the End of Power Transfer Register) to Tx and then sets this bit to "0"
 */
#define SENDEOP			BIT3

/*
 * If AP sets this bit to 1 then IDTP9220 M0 start the device authintication
 */
#define SEND_DEVICE_AUTH		BIT2

/*
 * If AP sets this bit to "1" then IDTP9220 M0 toggles LDO output once
 * (from on to off, or from off to on), and then sets this bit to "0"
 */
#define LDOTGL			BIT1
/* If AP sets this bit to "1" then IDTP9220 M0 sends the Proprietary Packet */
#define SENDPROPP		BIT0

/* bitmap for interrupt register 0x36 */
#define P922X_INT_ID_AUTH_SUCCESS	BIT13
#define P922X_INT_ID_AUTH_FAIL		BIT12
#define P922X_INT_DEVICE_AUTH_SUCCESS	BIT9
#define P922X_INT_DEVICE_AUTH_FAIL	BIT8
#define P922X_INT_TX_DATA_RECV		BIT4
#define P922X_INT_VRECT			BIT3
#define P922X_INT_OV_TEMP		BIT2
#define P922X_INT_OV_VOLT		BIT1
#define P922X_INT_OV_CURRENT		BIT0
#define P922X_INT_LIMIT_MASK		(P922X_INT_OV_TEMP | \
					P922X_INT_OV_VOLT | \
					P922X_INT_OV_CURRENT)

/* bitmap for customer command */
#define BC_NONE			0x00
#define BC_SET_FREQ		0x03
#define BC_GET_FREQ		0x04
#define BC_READ_FW_VER		0x05
#define BC_READ_Iin		0x06
#define BC_READ_Vin		0x07
#define BC_SET_Vin		0x0a
#define BC_ADAPTER_TYPE		0x0b
#define BC_RESET		0x0c
#define BC_READ_I2C		0x0d
#define BC_WRITE_I2C		0x0e
#define BC_VI2C_INIT		0x10

#define IDT_INT			"p9221-int"
#define IDT_PG			"p9221-pg"

#define OTP_START_ADDR       0x8000
#define OTP_VERIFY_ADDR     0x2590
#define OTP_VERIFY_VALUE    0x1414

#define SET_VOUT_VAL		6500
#define SET_VOUT_MAX		12500
#define SET_VOUT_MIN		3500

/* End of Power packet types */
#define EOP_OVER_TEMP		0x03
#define EOP_OVER_VOLT		0x04
#define EOP_OVER_CURRENT	0x05

#define FOD_COEF_ARRY_LENGTH	8
#define FOD_COEF_PARAM_LENGTH	16
#define SW_FOD_RECORD_SIZE	4

#define P922X_DIE_TEMP_DEFAULT	-273

/* iout calibration information */
#define IDME_OF_WPCIOUTCAL  "/idme/wpc_iout_cal"
#define IOUT_CAL_IDME_SIZE  14
#define IOUT_CAL_CHAR_SIZE  56
#define IOUT_CAL_THR_MAX    4
#define IOUT_CAL_AREA_MAX   5

#define REG_CAL_OFFSET0_ADDR 0x14
#define REG_CAL_OFFSET1_ADDR 0x18
#define REG_CAL_OFFSET2_ADDR 0x0A
#define REG_CAL_OFFSET3_ADDR 0x42
#define REG_CAL_OFFSET4_ADDR 0x66
#define REG_CAL_GAIN0_ADDR 0x12
#define REG_CAL_GAIN1_ADDR 0x16
#define REG_CAL_GAIN2_ADDR 0x1A
#define REG_CAL_GAIN3_ADDR 0x28
#define REG_CAL_GAIN4_ADDR 0x64
#define REG_CAL_THR0_ADDR 0x0c
#define REG_CAL_THR1_ADDR 0x0e
#define REG_CAL_THR2_ADDR 0x2c
#define REG_CAL_THR3_ADDR 0x2e

#define CAL_THR0_VAL 0x00c8 /* 200mA */
#define CAL_THR1_VAL 0x0190 /* 400mA */
#define CAL_THR2_VAL 0x0258 /* 600mA */
#define CAL_THR3_VAL 0x0320 /* 800mA */

#define GAIN_DEFAULT_VAL   0x00
#define OFFSET_DEFAULT_VAL 0x00

#define I2C_BYTES_MAX 255
#define PACKET_SIZE_MAX 27

#define DEFAULT_EPT_WORK_DELAY	30000	/* 30s */

enum charge_mode {
	CHARGE_5W_MODE,
	CHARGE_10W_MODE,
	CHARGE_MODE_MAX,
};

enum adapter_list {
	DOCK_ADAPTER_UNKNOWN     = 0x00,
	DOCK_ADAPTER_SDP     = 0x01,
	DOCK_ADAPTER_CDP     = 0x02,
	DOCK_ADAPTER_DCP     = 0x03,
	DOCK_ADAPTER_QC20        = 0x05,
	DOCK_ADAPTER_QC30        = 0x06,
	DOCK_ADAPTER_PD      = 0x07,
};

enum fod_type {
	TYPE_UNKNOWN = 0,
	TYPE_BPP,
	TYPE_BPP_PLUS,
	TYPE_MAX
};

struct idtp922xpgmtype {	/* write to structure at SRAM address 0x0400 */
	u16 status;		/* Read/Write by both 9220 and 9220 host */
	u16 startAddr;		/* OTP image address of the current packet */
	u16 codeLength;		/* The size of the OTP image data in the current packet */
	u16 dataChksum;		/* Checksum of the current packet */
	u8  dataBuf[128];	/* OTP image data of the current packet */
};

/* proprietary packet type */
struct propkt_type {
	u8 header;	/* The header consists of a single byte that indicates the Packet type. */
	u8 cmd;		/* Back channel command */
	u8 msg[5];	/* Send data buffer */
};

struct p922x_desc {
	const char *chg_dev_name;
	const char *alias_name;
};

struct p922x_switch_voltage {
	int voltage_low;
	int voltage_target;
	int voltage_high;
};

/*
 * In each RPP (Received Power Packet), RX will use
 * 'RPP = calcuatedPower * gain + offset' to report the power.
 */
struct p922x_fodcoeftype {
	u8 gain;
	u8 offs;
};

struct p922x_reg {
	u16 addr;
	u8  size;
};

struct p922x_cal_reg {
	u16 addr;
	u16 val;
};

struct p922x_iout_cal_data {
	struct p922x_cal_reg threshold[IOUT_CAL_THR_MAX];
	struct p922x_cal_reg gain[IOUT_CAL_AREA_MAX];
	struct p922x_cal_reg offset[IOUT_CAL_AREA_MAX];
};

#define IBUS_BUFFER_SIZE 6
struct adaptive_current_limit {
	int start_soc;
	uint32_t current_index;
	int fill_count;
	bool on_adaptive;

	int interval;
	int margin;
	int bpp_plus_max_ma;
	int max_current_limit;
	int min_current_limit;
	int start_ma;
	int ibus[IBUS_BUFFER_SIZE];
};

struct step_load {
	int start_ma;
	int step_ma;
	int step_max_ua;
	int bpp_plus_max_ua;
	uint32_t step_interval;	/* ms */
};

struct p922x_dev {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct p922x_desc *desc;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct mutex sys_lock;
	struct mutex irq_lock;
	struct mutex fod_lock;
	struct mutex lock;
	struct power_supply_desc wpc_desc;
	struct power_supply_config wpc_cfg;
	struct delayed_work wpc_init_work;
	struct p922x_reg reg;
	struct power_supply *wpc_psy;
	int int_gpio;
	int pg_gpio;
	int int_num;
	int pg_num;
	atomic_t online;
	bool use_buck;
	struct pinctrl *p922x_pinctrl;
	struct pinctrl_state *wpc_en0;
	struct pinctrl_state *wpc_en1;
	struct pinctrl_state *sleep_en0;
	struct pinctrl_state *sleep_en1;
	struct pinctrl_state *ovp_en0;
	struct pinctrl_state *ovp_en1;
	bool wpc_en;
	bool support_wpc_en;
	bool sleep_en;
	bool support_sleep_en;
	bool ovp_en;
	bool support_ovp_en;
	/* communication cap enable */
	bool cm_cap_en;

	/* fastcharge switch */
	bool tx_id_authen_status;
	bool tx_dev_authen_status;
	bool tx_authen_complete;
	struct delayed_work power_switch_work;
	enum power_supply_type chr_type;
	int switch_vth_low;
	int switch_vth_high;
	int wpc_mivr[CHARGE_MODE_MAX];
	struct p922x_switch_voltage switch_voltage[CHARGE_MODE_MAX];
	struct p922x_fodcoeftype bpp_5w_fod[FOD_COEF_ARRY_LENGTH];
	struct p922x_fodcoeftype bpp_10w_fod[FOD_COEF_ARRY_LENGTH];
	struct switch_dev dock_state;
	bool force_switch;
	bool is_hv_adapter;
	bool is_enabled;
	u8 tx_adapter_type;
	u8 over_reason;
	u8 bpp_5w_fod_num;
	u8 bpp_10w_fod_num;
	u8 dev_auth_retry;
	bool is_cal_data_ready;
	bool is_sram_updated;
	struct p922x_iout_cal_data *iout_cal_data;

	struct delayed_work adaptive_current_limit_work;
	struct charger_device *chg1_dev;
	struct adaptive_current_limit adaptive_current_limit;
	struct power_supply *bat_psy;
	int prev_input_current;
	struct mtk_charger *chg_info;
	int bpp_10w_default_input_current;

	struct step_load step_load;
	bool on_step_charging;
	bool step_load_en;
	struct delayed_work step_charging_work;
	struct timespec start_step_chg_time;

	struct delayed_work EPT_work;
	uint32_t EPT_work_delay;	/* unite: ms */

	int sw_fod_count;
	struct timespec sw_fod_time_record[SW_FOD_RECORD_SIZE];

	ktime_t pg_time[2];

	bool get_status_done;
};

struct p922x_ntc_temperature {
	signed int temperature;
	signed int voltage;
};

enum dock_state_type {
	TYPE_DOCKED = 7,
	TYPE_UNDOCKED = 8,
};

enum led_mode {
	LED_CONSTANT_ON = 99,
	LED_CONSTANT_OFF = 100,
};
static u16 fw_ver_list[] = {
	0x0303
};

/* The Output voltage(mv) corresponding to resistance temperature(degC) */
struct p922x_ntc_temperature ntc_temperature_table[] = {
	{-40, 1759}, {-39, 1756}, {-38, 1753}, {-37, 1749}, {-36, 1746},
	{-35, 1742}, {-34, 1738}, {-33, 1734}, {-32, 1730}, {-31, 1725},
	{-30, 1720}, {-29, 1715}, {-28, 1709}, {-27, 1704}, {-26, 1697},
	{-25, 1691}, {-24, 1684}, {-23, 1677}, {-22, 1670}, {-21, 1662},
	{-20, 1654}, {-19, 1646}, {-18, 1637}, {-17, 1628}, {-16, 1618},
	{-15, 1608}, {-14, 1598}, {-13, 1587}, {-12, 1575}, {-11, 1564},
	{-10, 1551}, { -9, 1539}, { -8, 1526}, { -7, 1512}, { -6, 1498},
	{ -5, 1484}, { -4, 1469}, { -3, 1454}, { -2, 1438}, { -1, 1422},
	{  0, 1405}, {  1, 1388}, {  2, 1371}, {  3, 1353}, {  4, 1335},
	{  5, 1316}, {  6, 1297}, {  7, 1278}, {  8, 1258}, {  9, 1238},
	{ 10, 1218}, { 11, 1198}, { 12, 1177}, { 13, 1156}, { 14, 1136},
	{ 15, 1114}, { 16, 1093}, { 17, 1072}, { 18, 1050}, { 19, 1029},
	{ 20, 1007}, { 21,  986}, { 22,  964}, { 23,  943}, { 24,  921},
	{ 25,  900}, { 26,  879}, { 27,  858}, { 28,  837}, { 29,  816},
	{ 30,  796}, { 31,  775}, { 32,  755}, { 33,  736}, { 34,  716},
	{ 35,  697}, { 36,  678}, { 37,  659}, { 38,  641}, { 39,  623},
	{ 40,  605}, { 41,  588}, { 42,  571}, { 43,  555}, { 44,  538},
	{ 45,  522}, { 46,  507}, { 47,  492}, { 48,  477}, { 49,  463},
	{ 50,  449}, { 51,  435}, { 52,  422}, { 53,  409}, { 54,  396},
	{ 55,  384}, { 56,  372}, { 57,  360}, { 58,  349}, { 59,  338},
	{ 60,  327}, { 61,  317}, { 62,  307}, { 63,  297}, { 64,  288},
	{ 65,  279}, { 66,  270}, { 67,  261}, { 68,  253}, { 69,  245},
	{ 70,  237}, { 71,  230}, { 72,  222}, { 73,  215}, { 74,  209},
	{ 75,  202}, { 76,  196}, { 77,  189}, { 78,  183}, { 79,  178},
	{ 80,  172}, { 81,  167}, { 82,  162}, { 83,  156}, { 84,  152},
	{ 85,  147}, { 86,  142}, { 87,  138}, { 88,  134}, { 89,  130},
	{ 90,  126}, { 91,  122}, { 92,  118}, { 93,  114}, { 94,  111},
	{ 95,  108}, { 96,  104}, { 97,  101}, { 98,   98}, { 99,   95},
	{100,   92}, {101,   90}, {102,   87}, {103,   84}, {104,   82},
	{105,   80}, {106,   77}, {107,   75}, {108,   73}, {109,   71},
	{110,   69}, {111,   67}, {112,   65}, {113,   63}, {114,   61},
	{115,   59}, {116,   58}, {117,   56}, {118,   55}, {119,   53},
	{120,   52}, {121,   50}, {122,   49}, {123,   47}, {124,   46},
	{125,   45}
};
#endif
