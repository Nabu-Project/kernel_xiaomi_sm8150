/*
 *  Sysfs interface for the universal power supply monitor class
 *
 *  Copyright © 2007  David Woodhouse <dwmw2@infradead.org>
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include "power_supply.h"

#undef dev_info
#define dev_info(x, ...)
#undef dev_dbg
#define dev_dbg(x, ...)
#undef dev_err
#define dev_err(x, ...)
#undef pr_info
#define pr_info(x, ...)
#undef pr_debug
#define pr_debug(x, ...)
#undef pr_error
#define pr_error(x, ...)
#undef printk
#define printk(x, ...)
#undef printk_deferred
#define printk_deferred(x, ...)

/*
 * This is because the name "current" breaks the device attr macro.
 * The "current" word resolves to "(get_current())" so instead of
 * "current" "(get_current())" appears in the sysfs.
 *
 * The source of this definition is the device.h which calls __ATTR
 * macro in sysfs.h which calls the __stringify macro.
 *
 * Only modification that the name is not tried to be resolved
 * (as a macro let's say).
 */

#define POWER_SUPPLY_ATTR(_name)					\
{									\
	.attr = { .name = #_name },					\
	.show = power_supply_show_property,				\
	.store = power_supply_store_property,				\
}

static struct device_attribute power_supply_attrs[];

static const char * const power_supply_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "USB_PD_DRP", "BrickID",
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5", "Wireless", "USB_FLOAT",
	"BMS", "Parallel", "Main", "Wipower", "USB_C_UFP", "USB_C_DFP",
	"Charge_Pump","ZIMI_CAR_POWER","Batt_Verify"
};

static const char * const power_supply_status_text[] = {
	"Unknown", "Charging", "Discharging", "Not charging", "Full"
};

static const char * const power_supply_charge_type_text[] = {
	"Unknown", "N/A", "Trickle", "Fast", "Taper"
};

static const char * const power_supply_health_text[] = {
	"Unknown", "Good", "Overheat", "Dead", "Over voltage",
	"Unspecified failure", "Cold", "Watchdog timer expire",
	"Safety timer expire", "Over current", "Warm", "Cool", "Hot"
};

static const char * const power_supply_technology_text[] = {
	"Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd",
	"LiMn"
};

static const char * const power_supply_capacity_level_text[] = {
	"Unknown", "Critical", "Low", "Normal", "High", "Full"
};

static const char * const power_supply_scope_text[] = {
	"Unknown", "System", "Device"
};

static const char * const power_supply_usbc_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static const char * const power_supply_usbc_pr_text[] = {
	"none", "dual power role", "sink", "source"
};

static const char * const power_supply_typec_src_rp_text[] = {
	"Rp-Default", "Rp-1.5A", "Rp-3A"
};

static ssize_t power_supply_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf) {
	ssize_t ret = 0;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	if (off == POWER_SUPPLY_PROP_TYPE) {
		value.intval = psy->desc->type;
	} else {
		ret = power_supply_get_property(psy, off, &value);

		if (ret < 0) {
			if (ret == -ENODATA)
				dev_dbg_ratelimited(dev,
					"driver has no data for `%s' property\n",
					attr->attr.name);
			else if (ret != -ENODEV && ret != -EAGAIN)
				dev_dbg_ratelimited(dev,
					"driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
			return ret;
		}
	}

	if (off == POWER_SUPPLY_PROP_STATUS)
		return sprintf(buf, "%s\n",
			       power_supply_status_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CHARGE_TYPE)
		return sprintf(buf, "%s\n",
			       power_supply_charge_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_HEALTH)
		return sprintf(buf, "%s\n",
			       power_supply_health_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TECHNOLOGY)
		return sprintf(buf, "%s\n",
			       power_supply_technology_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CAPACITY_LEVEL)
		return sprintf(buf, "%s\n",
			       power_supply_capacity_level_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPE ||
			off == POWER_SUPPLY_PROP_REAL_TYPE)
		return sprintf(buf, "%s\n",
			       power_supply_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_SCOPE)
		return sprintf(buf, "%s\n",
			       power_supply_scope_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPEC_MODE)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_usbc_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPEC_POWER_ROLE)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_usbc_pr_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPEC_SRC_RP)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_typec_src_rp_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_DIE_HEALTH)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_health_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CONNECTOR_HEALTH)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_health_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_SKIN_HEALTH)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_health_text[value.intval]);
	else if (off >= POWER_SUPPLY_PROP_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);
#if (defined CONFIG_BATT_VERIFY_BY_DS28E16 || defined CONFIG_BATT_VERIFY_BY_DS28E16_NABU)
	else if ((off == POWER_SUPPLY_PROP_ROMID) || (off == POWER_SUPPLY_PROP_DS_STATUS))
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			value.arrayval[0], value.arrayval[1], value.arrayval[2], value.arrayval[3],
			value.arrayval[4], value.arrayval[5], value.arrayval[6], value.arrayval[7]);
	else if ((off == POWER_SUPPLY_PROP_PAGE0_DATA) ||
			(off == POWER_SUPPLY_PROP_PAGE1_DATA) ||
			(off == POWER_SUPPLY_PROP_PAGEDATA))
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			value.arrayval[0], value.arrayval[1], value.arrayval[2], value.arrayval[3],
			value.arrayval[4], value.arrayval[5], value.arrayval[6], value.arrayval[7],
			value.arrayval[8], value.arrayval[9], value.arrayval[10], value.arrayval[11],
			value.arrayval[12], value.arrayval[13], value.arrayval[14], value.arrayval[15]);
	else if (off == POWER_SUPPLY_PROP_VERIFY_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);
#endif
	if (off == POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT)
		return sprintf(buf, "%lld\n", value.int64val);
	else if (off == POWER_SUPPLY_PROP_WIRELESS_VERSION)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_WIRELESS_WAKELOCK)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_SIGNAL_STRENGTH)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_WIRELESS_CP_EN)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_TYPE_RECHECK)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_TX_MAC)
		return scnprintf(buf, PAGE_SIZE, "%llx\n",
				value.int64val);
	else if (off == POWER_SUPPLY_PROP_PEN_MAC)
		return scnprintf(buf, PAGE_SIZE, "%llx\n",
				value.int64val);
	else if (off ==  POWER_SUPPLY_PROP_REVERSE_PEN_SOC)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off ==  POWER_SUPPLY_PROP_REVERSE_CHG_STATE)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_REVERSE_PEN_CHG_STATE)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				value.intval);
	else if (off == POWER_SUPPLY_PROP_RX_CR)
		return scnprintf(buf, PAGE_SIZE, "%llx\n",
				value.int64val);
	else if (off == POWER_SUPPLY_PROP_RX_CEP)
		return scnprintf(buf, PAGE_SIZE, "%llx\n",
				value.int64val);
	else if (off == POWER_SUPPLY_PROP_BT_STATE)
		return scnprintf(buf, PAGE_SIZE, "%x\n",
				value.intval);
	else
		return sprintf(buf, "%d\n", value.intval);
}

static ssize_t power_supply_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count) {
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;
	long val;
	int64_t num_long;

	/* maybe it is a enum property? */
	switch (off) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sysfs_match_string(power_supply_status_text, buf);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = sysfs_match_string(power_supply_charge_type_text, buf);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sysfs_match_string(power_supply_health_text, buf);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = sysfs_match_string(power_supply_technology_text, buf);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = sysfs_match_string(power_supply_capacity_level_text, buf);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = sysfs_match_string(power_supply_scope_text, buf);
		break;
	case POWER_SUPPLY_PROP_BT_STATE:
	case POWER_SUPPLY_PROP_RX_CR:
		ret = kstrtol(buf, 16, &val);
		if (ret < 0)
			return ret;
		ret = val;
		break;
	case POWER_SUPPLY_PROP_RX_CEP:
		ret = kstrtol(buf, 16, &val);
		if (ret < 0)
			return ret;
		ret = val;
		break;
	case POWER_SUPPLY_PROP_TX_MAC:
		ret = kstrtoll(buf, 16, &num_long);
		if (ret < 0)
			return ret;
		value.int64val = num_long;
		ret = power_supply_set_property(psy, off, &value);
		if (ret < 0)
			return ret;
		else
			return count;

		break;
	case POWER_SUPPLY_PROP_PEN_MAC:
		ret = kstrtoll(buf, 16, &num_long);
		if (ret < 0)
			return ret;
		value.int64val = num_long;
		ret = power_supply_set_property(psy, off, &value);
		if (ret < 0)
			return ret;
		else
			return count;

		break;
	default:
		ret = -EINVAL;
	}

	/*
	 * If no match was found, then check to see if it is an integer.
	 * Integer values are valid for enums in addition to the text value.
	 */
	if (ret < 0) {
		long long_val;

		ret = kstrtol(buf, 10, &long_val);
		if (ret < 0)
			return ret;

		ret = long_val;
	}

	value.intval = ret;

	ret = power_supply_set_property(psy, off, &value);
	if (ret < 0)
		return ret;

	return count;
}

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute power_supply_attrs[] = {
	/* Properties of type `int' */
	POWER_SUPPLY_ATTR(status),
	POWER_SUPPLY_ATTR(charge_type),
	POWER_SUPPLY_ATTR(health),
	POWER_SUPPLY_ATTR(present),
	POWER_SUPPLY_ATTR(online),
	POWER_SUPPLY_ATTR(authentic),
	POWER_SUPPLY_ATTR(technology),
	POWER_SUPPLY_ATTR(cycle_count),
	POWER_SUPPLY_ATTR(voltage_max),
	POWER_SUPPLY_ATTR(voltage_min),
	POWER_SUPPLY_ATTR(voltage_max_design),
	POWER_SUPPLY_ATTR(voltage_min_design),
	POWER_SUPPLY_ATTR(voltage_now),
	POWER_SUPPLY_ATTR(voltage_avg),
	POWER_SUPPLY_ATTR(voltage_ocv),
	POWER_SUPPLY_ATTR(voltage_boot),
	POWER_SUPPLY_ATTR(current_max),
	POWER_SUPPLY_ATTR(current_now),
	POWER_SUPPLY_ATTR(current_avg),
	POWER_SUPPLY_ATTR(current_boot),
	POWER_SUPPLY_ATTR(power_now),
	POWER_SUPPLY_ATTR(power_avg),
	POWER_SUPPLY_ATTR(charge_full_design),
	POWER_SUPPLY_ATTR(charge_empty_design),
	POWER_SUPPLY_ATTR(charge_full),
	POWER_SUPPLY_ATTR(charge_empty),
	POWER_SUPPLY_ATTR(charge_now),
	POWER_SUPPLY_ATTR(charge_avg),
	POWER_SUPPLY_ATTR(charge_counter),
	POWER_SUPPLY_ATTR(termination_current),
	POWER_SUPPLY_ATTR(ffc_termination_current),
	POWER_SUPPLY_ATTR(constant_charge_current),
	POWER_SUPPLY_ATTR(constant_charge_current_max),
	POWER_SUPPLY_ATTR(constant_charge_voltage),
	POWER_SUPPLY_ATTR(constant_charge_voltage_max),
	POWER_SUPPLY_ATTR(charge_control_limit),
	POWER_SUPPLY_ATTR(charge_control_limit_max),
	POWER_SUPPLY_ATTR(input_current_limit),
	POWER_SUPPLY_ATTR(energy_full_design),
	POWER_SUPPLY_ATTR(energy_empty_design),
	POWER_SUPPLY_ATTR(energy_full),
	POWER_SUPPLY_ATTR(energy_empty),
	POWER_SUPPLY_ATTR(energy_now),
	POWER_SUPPLY_ATTR(energy_avg),
	POWER_SUPPLY_ATTR(capacity),
	POWER_SUPPLY_ATTR(capacity_alert_min),
	POWER_SUPPLY_ATTR(capacity_alert_max),
	POWER_SUPPLY_ATTR(capacity_level),
	POWER_SUPPLY_ATTR(shutdown_delay),
	POWER_SUPPLY_ATTR(shutdown_delay_en),
	POWER_SUPPLY_ATTR(raw_capacity),
	POWER_SUPPLY_ATTR(soc_decimal),
	POWER_SUPPLY_ATTR(soc_decimal_rate),
	POWER_SUPPLY_ATTR(cold_thermal_level),
	POWER_SUPPLY_ATTR(temp),
	POWER_SUPPLY_ATTR(temp_max),
	POWER_SUPPLY_ATTR(temp_min),
	POWER_SUPPLY_ATTR(temp_alert_min),
	POWER_SUPPLY_ATTR(temp_alert_max),
	POWER_SUPPLY_ATTR(temp_ambient),
	POWER_SUPPLY_ATTR(temp_ambient_alert_min),
	POWER_SUPPLY_ATTR(temp_ambient_alert_max),
	POWER_SUPPLY_ATTR(time_to_empty_now),
	POWER_SUPPLY_ATTR(time_to_empty_avg),
	POWER_SUPPLY_ATTR(time_to_full_now),
	POWER_SUPPLY_ATTR(time_to_full_avg),
	POWER_SUPPLY_ATTR(type),
	POWER_SUPPLY_ATTR(scope),
	POWER_SUPPLY_ATTR(precharge_current),
	POWER_SUPPLY_ATTR(charge_term_current),
	POWER_SUPPLY_ATTR(calibrate),
	/* Local extensions */
	POWER_SUPPLY_ATTR(usb_hc),
	POWER_SUPPLY_ATTR(usb_otg),
	POWER_SUPPLY_ATTR(charge_enabled),
	POWER_SUPPLY_ATTR(set_ship_mode),
	POWER_SUPPLY_ATTR(real_type),
	POWER_SUPPLY_ATTR(hvdcp3_type),
	POWER_SUPPLY_ATTR(quick_charge_type),
	POWER_SUPPLY_ATTR(charge_now_raw),
	POWER_SUPPLY_ATTR(charge_now_error),
	POWER_SUPPLY_ATTR(capacity_raw),
	POWER_SUPPLY_ATTR(battery_charging_enabled),
	POWER_SUPPLY_ATTR(charging_enabled),
	POWER_SUPPLY_ATTR(step_charging_enabled),
	POWER_SUPPLY_ATTR(step_charging_step),
	POWER_SUPPLY_ATTR(pin_enabled),
	POWER_SUPPLY_ATTR(input_suspend),
	POWER_SUPPLY_ATTR(input_voltage_regulation),
	POWER_SUPPLY_ATTR(input_voltage_vrect),
	POWER_SUPPLY_ATTR(rx_iout),
	POWER_SUPPLY_ATTR(input_current_max),
	POWER_SUPPLY_ATTR(input_current_trim),
	POWER_SUPPLY_ATTR(input_current_settled),
	POWER_SUPPLY_ATTR(input_voltage_settled),
	POWER_SUPPLY_ATTR(bypass_vchg_loop_debouncer),
	POWER_SUPPLY_ATTR(charge_counter_shadow),
	POWER_SUPPLY_ATTR(hi_power),
	POWER_SUPPLY_ATTR(low_power),
	POWER_SUPPLY_ATTR(temp_cool),
	POWER_SUPPLY_ATTR(temp_warm),
	POWER_SUPPLY_ATTR(temp_cold),
	POWER_SUPPLY_ATTR(temp_hot),
	POWER_SUPPLY_ATTR(system_temp_level),
	POWER_SUPPLY_ATTR(resistance),
	POWER_SUPPLY_ATTR(resistance_capacitive),
	POWER_SUPPLY_ATTR(resistance_id),
	POWER_SUPPLY_ATTR(resistance_now),
	POWER_SUPPLY_ATTR(flash_current_max),
	POWER_SUPPLY_ATTR(update_now),
	POWER_SUPPLY_ATTR(esr_count),
	POWER_SUPPLY_ATTR(buck_freq),
	POWER_SUPPLY_ATTR(boost_current),
	POWER_SUPPLY_ATTR(safety_timer_enabled),
	POWER_SUPPLY_ATTR(charge_done),
	POWER_SUPPLY_ATTR(flash_active),
	POWER_SUPPLY_ATTR(flash_trigger),
	POWER_SUPPLY_ATTR(force_tlim),
	POWER_SUPPLY_ATTR(dp_dm),
	POWER_SUPPLY_ATTR(dp_dm_bq),
	POWER_SUPPLY_ATTR(input_current_limited),
	POWER_SUPPLY_ATTR(input_current_now),
	POWER_SUPPLY_ATTR(charge_qnovo_enable),
	POWER_SUPPLY_ATTR(current_qnovo),
	POWER_SUPPLY_ATTR(voltage_qnovo),
	POWER_SUPPLY_ATTR(rerun_aicl),
	POWER_SUPPLY_ATTR(cycle_count_id),
	POWER_SUPPLY_ATTR(safety_timer_expired),
	POWER_SUPPLY_ATTR(restricted_charging),
	POWER_SUPPLY_ATTR(current_capability),
	POWER_SUPPLY_ATTR(typec_mode),
	POWER_SUPPLY_ATTR(typec_cc_orientation),
	POWER_SUPPLY_ATTR(typec_power_role),
	POWER_SUPPLY_ATTR(typec_src_rp),
	POWER_SUPPLY_ATTR(pd_allowed),
	POWER_SUPPLY_ATTR(pd_active),
	POWER_SUPPLY_ATTR(pd_authentication),
	POWER_SUPPLY_ATTR(pd_remove_compensation),
	POWER_SUPPLY_ATTR(pd_in_hard_reset),
	POWER_SUPPLY_ATTR(pd_current_max),
	POWER_SUPPLY_ATTR(apdo_max),
	POWER_SUPPLY_ATTR(pd_usb_suspend_supported),
	POWER_SUPPLY_ATTR(charger_temp),
	POWER_SUPPLY_ATTR(charger_temp_max),
	POWER_SUPPLY_ATTR(parallel_disable),
	POWER_SUPPLY_ATTR(pe_start),
	POWER_SUPPLY_ATTR(soc_reporting_ready),
	POWER_SUPPLY_ATTR(debug_battery),
	POWER_SUPPLY_ATTR(fcc_delta),
	POWER_SUPPLY_ATTR(icl_reduction),
	POWER_SUPPLY_ATTR(parallel_mode),
	POWER_SUPPLY_ATTR(die_health),
	POWER_SUPPLY_ATTR(connector_health),
	POWER_SUPPLY_ATTR(connector_temp),
	POWER_SUPPLY_ATTR(vbus_disable),
	POWER_SUPPLY_ATTR(ctm_current_max),
	POWER_SUPPLY_ATTR(hw_current_max),
	POWER_SUPPLY_ATTR(pr_swap),
	POWER_SUPPLY_ATTR(cc_step),
	POWER_SUPPLY_ATTR(cc_step_sel),
	POWER_SUPPLY_ATTR(sw_jeita_enabled),
	POWER_SUPPLY_ATTR(pd_voltage_max),
	POWER_SUPPLY_ATTR(pd_voltage_min),
	POWER_SUPPLY_ATTR(sdp_current_max),
	POWER_SUPPLY_ATTR(dc_thermal_levels),
	POWER_SUPPLY_ATTR(connector_type),
	POWER_SUPPLY_ATTR(parallel_batfet_mode),
	POWER_SUPPLY_ATTR(parallel_fcc_max),
	POWER_SUPPLY_ATTR(wireless_version),
	POWER_SUPPLY_ATTR(wireless_fw_version),
	POWER_SUPPLY_ATTR(signal_strength),
	POWER_SUPPLY_ATTR(wireless_cp_en),
	POWER_SUPPLY_ATTR(wireless_power_good_en),
	POWER_SUPPLY_ATTR(sw_disabel_dc_en),
	POWER_SUPPLY_ATTR(wireless_wakelock),
	POWER_SUPPLY_ATTR(wireless_tx_id),
	POWER_SUPPLY_ATTR(tx_adapter),
	POWER_SUPPLY_ATTR(tx_mac),
	POWER_SUPPLY_ATTR(rx_cr),
	POWER_SUPPLY_ATTR(rx_cep),
	POWER_SUPPLY_ATTR(bt_state),
	POWER_SUPPLY_ATTR(pen_mac),
	POWER_SUPPLY_ATTR(min_icl),
	POWER_SUPPLY_ATTR(moisture_detected),
	POWER_SUPPLY_ATTR(batt_profile_version),
	POWER_SUPPLY_ATTR(batt_full_current),
	POWER_SUPPLY_ATTR(battery_charging_limited),
	POWER_SUPPLY_ATTR(slowly_charging),
	POWER_SUPPLY_ATTR(bq_input_suspend),
	POWER_SUPPLY_ATTR(recharge_soc),
	POWER_SUPPLY_ATTR(recharge_vbat),
	POWER_SUPPLY_ATTR(sys_termination_current),
	POWER_SUPPLY_ATTR(ffc_sys_termination_current),
	POWER_SUPPLY_ATTR(vbatt_full_vol),
	POWER_SUPPLY_ATTR(fcc_vbatt_full_vol),
	POWER_SUPPLY_ATTR(ki_coeff_current),
#if (defined CONFIG_BATT_VERIFY_BY_DS28E16 || defined CONFIG_BATT_VERIFY_BY_DS28E16_NABU)
	/* battery verify properties */
	POWER_SUPPLY_ATTR(romid),
	POWER_SUPPLY_ATTR(ds_status),
	POWER_SUPPLY_ATTR(pagenumber),
	POWER_SUPPLY_ATTR(pagedata),
	POWER_SUPPLY_ATTR(authen_result),
	POWER_SUPPLY_ATTR(session_seed),
	POWER_SUPPLY_ATTR(s_secret),
	POWER_SUPPLY_ATTR(challenge),
	POWER_SUPPLY_ATTR(auth_anon),
	POWER_SUPPLY_ATTR(auth_bdconst),
	POWER_SUPPLY_ATTR(page0_data),
	POWER_SUPPLY_ATTR(page1_data),
	POWER_SUPPLY_ATTR(verify_model_name),
	POWER_SUPPLY_ATTR(maxim_batt_cycle_count),
#endif
	POWER_SUPPLY_ATTR(hvdcp_opti_allowed),
	POWER_SUPPLY_ATTR(fastcharge_mode),
	POWER_SUPPLY_ATTR(smb_en_mode),
	POWER_SUPPLY_ATTR(smb_en_reason),
	POWER_SUPPLY_ATTR(esr_actual),
	POWER_SUPPLY_ATTR(esr_nominal),
	POWER_SUPPLY_ATTR(soh),
	POWER_SUPPLY_ATTR(clear_soh),
	POWER_SUPPLY_ATTR(force_recharge),
	POWER_SUPPLY_ATTR(fcc_stepper_enable),
	POWER_SUPPLY_ATTR(toggle_stat),
	POWER_SUPPLY_ATTR(type_recheck),
	POWER_SUPPLY_ATTR(liquid_detection),
	POWER_SUPPLY_ATTR(dynamic_fv_enabled),
	POWER_SUPPLY_ATTR(main_fcc_max),
	POWER_SUPPLY_ATTR(fg_reset),
	POWER_SUPPLY_ATTR(qc_opti_disable),
	POWER_SUPPLY_ATTR(cc_soc),
	POWER_SUPPLY_ATTR(batt_age_level),
	POWER_SUPPLY_ATTR(voltage_vph),
	POWER_SUPPLY_ATTR(chip_version),
	POWER_SUPPLY_ATTR(chip_ok),
	POWER_SUPPLY_ATTR(therm_icl_limit),
	POWER_SUPPLY_ATTR(dc_reset),
	POWER_SUPPLY_ATTR(scale_mode_en),
	POWER_SUPPLY_ATTR(voltage_max_limit),
	POWER_SUPPLY_ATTR(real_capacity),
	POWER_SUPPLY_ATTR(esr_sw_control),
	POWER_SUPPLY_ATTR(force_main_icl),
	POWER_SUPPLY_ATTR(force_main_fcc),
	POWER_SUPPLY_ATTR(comp_clamp_level),
	POWER_SUPPLY_ATTR(adapter_cc_mode),
	POWER_SUPPLY_ATTR(skin_health),
	POWER_SUPPLY_ATTR(apsd_rerun),
	POWER_SUPPLY_ATTR(apsd_timeout),
	/* Charge pump properties */
	POWER_SUPPLY_ATTR(cp_status1),
	POWER_SUPPLY_ATTR(cp_status2),
	POWER_SUPPLY_ATTR(cp_enable),
	POWER_SUPPLY_ATTR(cp_switcher_en),
	POWER_SUPPLY_ATTR(cp_die_temp),
	POWER_SUPPLY_ATTR(cp_isns),
	POWER_SUPPLY_ATTR(cp_isns_slave),
	POWER_SUPPLY_ATTR(cp_toggle_switcher),
	POWER_SUPPLY_ATTR(cp_irq_status),
	POWER_SUPPLY_ATTR(cp_ilim),
	POWER_SUPPLY_ATTR(step_index),
	/* Bq charge pump properties */
	POWER_SUPPLY_ATTR(ti_battery_present),
	POWER_SUPPLY_ATTR(ti_vbus_present),
	POWER_SUPPLY_ATTR(ti_battery_voltage),
	POWER_SUPPLY_ATTR(ti_battery_current),
	POWER_SUPPLY_ATTR(ti_battery_temperature),
	POWER_SUPPLY_ATTR(ti_bus_voltage),
	POWER_SUPPLY_ATTR(ti_bus_current),
	POWER_SUPPLY_ATTR(ti_bus_temperature),
	POWER_SUPPLY_ATTR(ti_die_temperature),
	POWER_SUPPLY_ATTR(ti_alarm_status),
	POWER_SUPPLY_ATTR(ti_fault_status),
	POWER_SUPPLY_ATTR(ti_reg_status),
	POWER_SUPPLY_ATTR(ti_set_bus_protection_for_qc3),
	POWER_SUPPLY_ATTR(ti_bus_error_status),
	POWER_SUPPLY_ATTR(ti_charge_mode),
	POWER_SUPPLY_ATTR(ti_bypass_mode_enable),
	POWER_SUPPLY_ATTR(irq_status),
	POWER_SUPPLY_ATTR(parallel_output_mode),
	/* DIV 2 properties */
	POWER_SUPPLY_ATTR(div_2_mode),
	POWER_SUPPLY_ATTR(reverse_chg_mode),
	POWER_SUPPLY_ATTR(reverse_chg_state),
	POWER_SUPPLY_ATTR(reverse_pen_chg_state),
	POWER_SUPPLY_ATTR(reverse_gpio_state),
	POWER_SUPPLY_ATTR(reset_div_2_mode),
	POWER_SUPPLY_ATTR(aicl_enable),
	POWER_SUPPLY_ATTR(otg_state),
	POWER_SUPPLY_ATTR(reverse_chg_hall3),
	POWER_SUPPLY_ATTR(reverse_chg_hall4),
	POWER_SUPPLY_ATTR(reverse_pen_soc),
	POWER_SUPPLY_ATTR(reverse_vout),
	POWER_SUPPLY_ATTR(reverse_iout),
	POWER_SUPPLY_ATTR(rx_op_ble),
	POWER_SUPPLY_ATTR(op_ble),
	
	/* Local extensions of type int64_t */
	POWER_SUPPLY_ATTR(charge_counter_ext),
	/* Properties of type `const char *' */
	POWER_SUPPLY_ATTR(model_name),
	POWER_SUPPLY_ATTR(manufacturer),
	POWER_SUPPLY_ATTR(serial_number),
	POWER_SUPPLY_ATTR(battery_type),
	POWER_SUPPLY_ATTR(cycle_counts),
};

static struct attribute *
__power_supply_attrs[ARRAY_SIZE(power_supply_attrs) + 1];

static umode_t power_supply_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct power_supply *psy = dev_get_drvdata(dev);
	umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;

	if (attrno == POWER_SUPPLY_PROP_TYPE)
		return mode;

	for (i = 0; i < psy->desc->num_properties; i++) {
		int property = psy->desc->properties[i];

		if (property == attrno) {
			if (psy->desc->property_is_writeable &&
			    psy->desc->property_is_writeable(psy, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	return 0;
}

static struct attribute_group power_supply_attr_group = {
	.attrs = __power_supply_attrs,
	.is_visible = power_supply_attr_is_visible,
};

static const struct attribute_group *power_supply_attr_groups[] = {
	&power_supply_attr_group,
	NULL,
};

void power_supply_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = power_supply_attr_groups;

	for (i = 0; i < ARRAY_SIZE(power_supply_attrs); i++)
		__power_supply_attrs[i] = &power_supply_attrs[i].attr;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

int power_supply_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	if (!psy || !psy->desc) {
		dev_dbg(dev, "No power supply yet\n");
		return ret;
	}

	ret = add_uevent_var(env, "POWER_SUPPLY_NAME=%s", psy->desc->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < psy->desc->num_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &power_supply_attrs[psy->desc->properties[j]];

		ret = power_supply_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			/* When a battery is absent, we expect -ENODEV. Don't abort;
			   send the uevent with at least the the PRESENT=0 property */
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		ret = add_uevent_var(env, "POWER_SUPPLY_%s=%s", attrname, prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}
