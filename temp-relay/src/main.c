#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define SLEEP_TIME_MS 1000

static const struct gpio_dt_spec led_alive = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);
static const struct gpio_dt_spec alt_temp = GPIO_DT_SPEC_GET(DT_ALIAS(alt_temp), gpios);
static const struct i2c_dt_spec i2c_temp = I2C_DT_SPEC_GET(DT_ALIAS(i2c_temp));

// TODO: move into Kconfig
#define CONFIG_COMMS_STACK_SIZE 500
#define CONFIG_COMMS_PRIORITY 5

#define CONFIG_INFO_STACK_SIZE 500
#define CONFIG_INFO_PRIORITY 5
#define CONFIG_INFO_ALIVE_PERIOD_MS 1000

#define CONFIG_MONITOR_STACK_SIZE 500
#define CONFIG_MONITOR_PRIORITY 5

static const uint8_t TMP102_REG_CURR = 0x00;
static const uint8_t TMP102_REG_CONF = 0x01;
static const uint8_t TMP102_REG_LOW = 0x02;
static const uint8_t TMP102_REG_HIGH = 0x03;

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
		return -ENODEV;
	}

	buf[0] = TMP102_REG_CONF;
	buf[1] = 0;

	err = i2c_write_read_dt(&i2c_temp, buf, 1, buf, sizeof(buf));
	*config = (buf[0] << 8) | buf[1];

	return err;
}

static int temp_write_config(const uint16_t config)
{
	uint8_t buf[3];

	if (!i2c_is_ready_dt(&i2c_temp)) {
		return -ENODEV;
	}

	buf[0] = TMP102_REG_CONF;
	buf[1] = config >> 8;
	buf[2] = config & 0xFF;

	return i2c_write_dt(&i2c_temp, buf, sizeof(buf));
}

static int temp_read(const uint8_t reg, float *temp)
{
	int err;
	uint8_t buf[2];

	if (!i2c_is_ready_dt(&i2c_temp)) {
		return -ENODEV;
	}

	buf[0] = reg;
	buf[1] = 0;

	err = i2c_write_read_dt(&i2c_temp, buf, 1, buf, sizeof(buf));
	*temp = temp_from_bits(buf[0], buf[1]);

	return err;
}

static int temp_write(const uint8_t reg, const float temp)
{
	uint8_t buf[3];
	int16_t temp_bits;

	if (!i2c_is_ready_dt(&i2c_temp)) {
		return -ENODEV;
	}

	temp_bits = temp_to_bits(temp);
	buf[0] = reg;
	buf[1] = (uint8_t)(temp_bits >> 4);
	buf[2] = (uint8_t)(temp_bits << 4);

	return i2c_write_dt(&i2c_temp, buf, sizeof(buf));
}

static void comms_run_thread()
{
	while (true) {
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(comms_thread, CONFIG_COMMS_STACK_SIZE, comms_run_thread, NULL, NULL, NULL, CONFIG_COMMS_PRIORITY, 0, 0);

struct k_poll_signal info_signal;
struct k_poll_event info_events[1];
#define INFO_ALIVE_SIGNAL 0x42

struct k_timer info_timer;

void info_run_timer()
{
	k_poll_signal_raise(&info_signal, INFO_ALIVE_SIGNAL);
}

static void info_init_gpio(void)
{
	int err;

	err = gpio_is_ready_dt(&led_alive) ? 0 : -EBUSY;
	if (!err) {
		err = gpio_pin_configure_dt(&led_alive, GPIO_OUTPUT_ACTIVE);
	}

	__ASSERT_NO_MSG(!err);
}

static void info_init_signals(void)
{
	k_poll_signal_init(&info_signal);
	k_poll_event_init(&info_events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &info_signal);
}

static void info_init_timer(void)
{
	k_timer_init(&info_timer, info_run_timer, NULL);
	k_timer_start(&info_timer, K_NO_WAIT, K_MSEC(CONFIG_INFO_ALIVE_PERIOD_MS));
}

static void info_run_thread()
{
	int sig_state = 0, sig_result = 0;

	info_init_gpio();
	info_init_signals();
	info_init_timer();

	while (true) {
		k_poll(info_events, ARRAY_SIZE(info_events), K_FOREVER);

		if (info_events[0].state == K_POLL_STATE_SIGNALED) {
			k_poll_signal_check(&info_signal, &sig_state, &sig_result);
			k_poll_signal_reset(&info_signal);
			info_events[0].state = K_POLL_STATE_NOT_READY;
		}

		if (sig_state && sig_result == INFO_ALIVE_SIGNAL) {
			gpio_pin_toggle_dt(&led_alive);
		}
	}
}

K_THREAD_DEFINE(info_thread, CONFIG_INFO_STACK_SIZE, info_run_thread, NULL, NULL, NULL, CONFIG_INFO_PRIORITY, 0, 0);

static struct gpio_callback alt_temp_cb_data;

static void monitor_alt_temp(const struct device *dev, struct gpio_callback *cb, const uint32_t pins)
{
	printf("Alert!\n");
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
		printf("Low on init: %u.%02u C\n", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	temp_write(TMP102_REG_LOW, 29.0);
	if (!temp_read(TMP102_REG_LOW, &temp)) {
		printf("Low now: %u.%02u C\n", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	if (!temp_read(TMP102_REG_HIGH, &temp)) {
		printf("High on init: %u.%02u C\n", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}
	temp_write(TMP102_REG_HIGH, 30.0);
	if (!temp_read(TMP102_REG_HIGH, &temp)) {
		printf("High now: %u.%02u C\n", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
	}

	if (!temp_read_config(&config)) {
		printf("Config on init: 0x%x\n", config);
	}
	if (!temp_write_config(config | (1 << 10))) {
		printf("Config updated...\n");
	}
	if (!temp_read_config(&config)) {
		printf("Config now: 0x%x\n", config);
	}

	while (true) {
		if (!temp_read(TMP102_REG_CURR, &temp)) {
			printf("Current: %u.%02u C\n", ((unsigned int)temp / 100), ((unsigned int)temp % 100));
		}
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(monitor_thread, CONFIG_MONITOR_STACK_SIZE, monitor_run_thread, NULL, NULL, NULL,
		CONFIG_MONITOR_PRIORITY, 0, 0);