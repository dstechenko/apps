#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(monitor, CONFIG_MONITOR_LOG_LEVEL);

#define TMP102_REG_CURR 0x00
#define TMP102_REG_CONF 0x01
#define TMP102_REG_LOW 0x02
#define TMP102_REG_HIGH 0x03

static struct gpio_callback alt_temp_cb_data;
static const struct gpio_dt_spec alt_temp = GPIO_DT_SPEC_GET(DT_ALIAS(alt_temp), gpios);
static const struct i2c_dt_spec i2c_temp = I2C_DT_SPEC_GET(DT_ALIAS(i2c_temp));

static float temp_from_bits(const uint8_t msb, const uint8_t lsb)
{
	int16_t val = ((int16_t)msb << 4) | (lsb >> 4);
	if (val > 0x7FF) {
		val |= 0xF000;
	}
	return (val * 0.0625) * 100;
}

static int16_t temp_to_bits(const float temp)
{
	int16_t bits = (int16_t)((double)temp / 0.0625);
	if (bits < 0) {
		bits &= 0x0FFF;
	}
	return bits;
}

static int temp_read_config(uint16_t *config)
{
	int err;
	uint8_t buf[2];

	if (!i2c_is_ready_dt(&i2c_temp)) {
		LOG_ERR("i2c temp module not ready");
		return -ENODEV;
	}

	buf[0] = TMP102_REG_CONF;
	buf[1] = 0;

	err = i2c_write_read_dt(&i2c_temp, buf, 1, buf, sizeof(buf));
	*config = (buf[0] << 8) | buf[1];

	if (err) {
		LOG_ERR("i2c temp module err: %d", err);
	}

	return err;
}

static int temp_write_config(const uint16_t config)
{
	int err;
	uint8_t buf[3];

	if (!i2c_is_ready_dt(&i2c_temp)) {
		LOG_ERR("i2c temp module not ready");
		return -ENODEV;
	}

	buf[0] = TMP102_REG_CONF;
	buf[1] = config >> 8;
	buf[2] = config & 0xFF;
	err = i2c_write_dt(&i2c_temp, buf, sizeof(buf));

	if (err) {
		LOG_ERR("i2c temp module err: %d", err);
	}

	return err;
}

static int temp_read(const uint8_t reg, float *temp)
{
	int err;
	uint8_t buf[2];

	if (!i2c_is_ready_dt(&i2c_temp)) {
		LOG_ERR("i2c temp module not ready");
		return -ENODEV;
	}

	buf[0] = reg;
	buf[1] = 0;

	err = i2c_write_read_dt(&i2c_temp, buf, 1, buf, sizeof(buf));
	*temp = temp_from_bits(buf[0], buf[1]);

	if (err) {
		LOG_ERR("i2c temp module err: %d", err);
	}

	return err;
}

static int temp_write(const uint8_t reg, const float temp)
{
	int err;
	uint8_t buf[3];
	int16_t temp_bits;

	if (!i2c_is_ready_dt(&i2c_temp)) {
		LOG_ERR("i2c temp module not ready");
		return -ENODEV;
	}

	temp_bits = temp_to_bits(temp);
	buf[0] = reg;
	buf[1] = (uint8_t)(temp_bits >> 4);
	buf[2] = (uint8_t)(temp_bits << 4);
	err = i2c_write_dt(&i2c_temp, buf, sizeof(buf));

	if (err) {
		LOG_ERR("i2c temp module err: %d", err);
	}

	return err;
}

static void monitor_alt_temp(const struct device *dev, struct gpio_callback *cb, const uint32_t pins)
{
	LOG_INF("alert!");
}

static void monitor_init_gpio(void)
{
	int err;

	err = gpio_is_ready_dt(&alt_temp) ? 0 : -EBUSY;
	if (!err) {
		err = gpio_pin_configure_dt(&alt_temp, GPIO_INPUT);
	}
	__ASSERT_NO_MSG(!err);

	if (!err) {
		err = gpio_pin_interrupt_configure_dt(&alt_temp, GPIO_INT_EDGE_TO_ACTIVE);
	}
	__ASSERT_NO_MSG(!err);

	if (!err) {
		gpio_init_callback(&alt_temp_cb_data, monitor_alt_temp, BIT(alt_temp.pin));
		gpio_add_callback(alt_temp.port, &alt_temp_cb_data);
	}
}

static void monitor_run_thread()
{
	uint16_t config = 0;
	float temp = 0;

	monitor_init_gpio();

	if (!temp_read(TMP102_REG_LOW, &temp)) {
		LOG_INF("low on init: %u.%02u C", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	temp_write(TMP102_REG_LOW, 30.0);
	if (!temp_read(TMP102_REG_LOW, &temp)) {
		LOG_INF("low now: %u.%02u C", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	if (!temp_read(TMP102_REG_HIGH, &temp)) {
		LOG_INF("high on init: %u.%02u C", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	temp_write(TMP102_REG_HIGH, 31.0);
	if (!temp_read(TMP102_REG_HIGH, &temp)) {
		LOG_INF("high now: %u.%02u C", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}

	if (!temp_read_config(&config)) {
		LOG_INF("config on init: 0x%x", config);
	}
	if (!temp_write_config(config | (1 << 10))) {
		LOG_INF("config updated...");
	}
	if (!temp_read_config(&config)) {
		LOG_INF("config now: 0x%x", config);
	}

	while (true) {
		LOG_INF("thread is alive...");
		if (!temp_read(TMP102_REG_CURR, &temp)) {
			LOG_INF("current: %u.%02u C", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
		}
		k_msleep(CONFIG_MONITOR_ALIVE_LOG_PERIOD_MS);
	}
}

K_THREAD_DEFINE(monitor_thread, CONFIG_MONITOR_STACK_SIZE, monitor_run_thread, NULL, NULL, NULL,
		CONFIG_MONITOR_PRIORITY, 0, 0);