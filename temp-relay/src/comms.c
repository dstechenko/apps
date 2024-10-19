#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#define SLEEP_TIME_MS 1000

#define CONFIG_COMMS_STACK_SIZE 500
#define CONFIG_COMMS_PRIORITY 5

static void comms_run_thread()
{
	while (true) {
		k_msleep(SLEEP_TIME_MS);
	}
}

K_THREAD_DEFINE(comms_thread, CONFIG_COMMS_STACK_SIZE, comms_run_thread, NULL, NULL, NULL, CONFIG_COMMS_PRIORITY, 0, 0);