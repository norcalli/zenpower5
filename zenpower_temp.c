/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * zenpower - Temperature monitoring backend
 *
 * Temperature measurements via SMN registers.
 * Used by all Zen generations.
 *
 * Supports Tctl (control temp) and per-CCD temperatures.
 */

#include "zenpower.h"

#define F17H_M01H_REPORTED_TEMP_CTRL        0x00059800
#define F17H_TEMP_ADJUST_MASK               0x80000
#define ZEN_CCD_TEMP_VALID                  BIT(11)
#define ZEN_CCD_TEMP_MASK                   0x7ff  /* GENMASK(10, 0) */

/* Returns millidegrees C. Signed: the -49000 offset legitimately pushes cold
 * readings negative, and computing that in unsigned int wraps to ~4.29e9,
 * which hwmon then reports as a multi-million-degree temperature.
 */
long zenpower_temp_get_ctl(struct zenpower_data *data)
{
	long temp;
	u32 regval;

	data->read_amdsmn_addr(data->pdev, data->node_id,
							F17H_M01H_REPORTED_TEMP_CTRL, &regval);
	temp = (long)(regval >> 21) * 125;
	if (regval & F17H_TEMP_ADJUST_MASK)
		temp -= 49000;
	return temp;
}

long zenpower_temp_get_ccd(struct zenpower_data *data, u32 ccd_addr)
{
	u32 regval;
	data->read_amdsmn_addr(data->pdev, data->node_id, ccd_addr, &regval);

	/* Check if CCD temperature is valid */
	if (!(regval & ZEN_CCD_TEMP_VALID))
		return 0;

	return (long)(regval & ZEN_CCD_TEMP_MASK) * 125 - 49000;
}

/* Probe-time test for whether ccd_addr really is a CCD temperature register.
 *
 * The valid bit alone is not sufficient: on parts where the CCD temperature
 * base address is wrong for the model, the register we land on is some other
 * SMN register that happens to have BIT(11) set, and it decodes to a wildly
 * out-of-range temperature. Require the decoded value to be physically
 * plausible before exposing the sensor.
 */
bool zenpower_temp_ccd_present(struct zenpower_data *data, u32 ccd_addr)
{
	u32 regval;
	long temp;

	data->read_amdsmn_addr(data->pdev, data->node_id, ccd_addr, &regval);

	if (!(regval & ZEN_CCD_TEMP_VALID))
		return false;

	temp = (long)(regval & ZEN_CCD_TEMP_MASK) * 125 - 49000;

	return temp >= ZEN_CCD_TEMP_MIN_MC && temp <= ZEN_CCD_TEMP_MAX_MC;
}
