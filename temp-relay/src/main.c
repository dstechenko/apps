#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define SLEEP_TIME_MS 2000

static const struct gpio_dt_spec led_alive = GPIO_DT_SPEC_GET(DT_ALIAS(led_alive), gpios);
static const struct i2c_dt_spec i2c_temp = I2C_DT_SPEC_GET(DT_ALIAS(i2c_temp));

// TODO: move into Kconfig
#define CONFIG_COMMS_STACK_SIZE 500
#define CONFIG_COMMS_PRIORITY 5

#define CONFIG_INFO_STACK_SIZE 500
#define CONFIG_INFO_PRIORITY 5
#define CONFIG_INFO_ALIVE_PERIOD_MS 1000

#define CONFIG_MONITOR_STACK_SIZE 500
#define CONFIG_MONITOR_PRIORITY 5

static const uint8_t TMP102_ADDR_READ = 0x48 << 1;
static const uint8_t TMP102_ADDR_WRITE = TMP102_ADDR_READ | 0x01;
static const uint8_t TMP102_REG_TCUR = 0x00;
static const uint8_t TMP102_REG_CONF = 0x01;
static const uint8_t TMP102_REG_TLOW = 0x02;
static const uint8_t TMP102_REG_THIGH = 0x03;

extern float temp_from_bits(const uint8_t msb, const uint8_t lsb)
{
	int16_t val = ((int16_t)msb << 4) | (lsb >> 4);
	if (val > 0x7FF) {
		val |= 0xF000;
	}
	return (val * 0.0625) * 100;
}

extern uint16_t temp_to_bits(const float temp)
{
	int16_t bits = (int16_t)(temp / 0.0625);
	if (bits < 0) {
		bits &= 0x0FFF;
	}
	return bits;
}

extern int temp_read_config(uint16_t *config)
{
	int err;
	uint8_t buf[2] = { TMP102_REG_CONF, 0 };

	if (!i2c_is_ready_dt(&i2c_temp)) {
		return -ENODEV;
	}

	err = i2c_write_read_dt(&i2c_temp, buf, 1, buf, sizeof(buf));
	*config = (buf[0] << 8) | buf[1];

	return err;
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
	int sig_state, sig_result;
	bool led_state = true;

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
			led_state = !led_state;
			printf("LED state: %s\n", led_state ? "ON" : "OFF");
			uint16_t config = 0;
			if (!temp_read_config(&config)) {
				printf("Config: 0x%x\n", config);
			}
		}
	}
}

K_THREAD_DEFINE(info_thread, CONFIG_INFO_STACK_SIZE, info_run_thread, NULL, NULL, NULL, CONFIG_INFO_PRIORITY, 0, 0);

static void monitor_run_thread()
{
	while (true) {
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(monitor_thread, CONFIG_MONITOR_STACK_SIZE, monitor_run_thread, NULL, NULL, NULL,
		CONFIG_MONITOR_PRIORITY, 0, 0);