#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(comms, CONFIG_COMMS_LOG_LEVEL);

static void comms_run_thread()
{
	while (true) {
		LOG_INF("thread is alive...");
		k_msleep(CONFIG_COMMS_ALIVE_LOG_PERIOD_MS);
	}
}

K_THREAD_DEFINE(comms_thread, CONFIG_COMMS_STACK_SIZE, comms_run_thread, NULL, NULL, NULL, CONFIG_COMMS_PRIORITY, 0, 0);