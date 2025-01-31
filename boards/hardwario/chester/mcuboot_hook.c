#include <bootutil/mcuboot_status.h>
#include <stdio.h>
#include <assert.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(mcuboot_hook, 4);

#define STATUS_TASK_STACK_SIZE 2048

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static mcuboot_status_type_t mcuboot_status = MCUBOOT_STATUS_STARTUP;

static void dispatcher_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
	gpio_pin_set_dt(&led0, 0);

	gpio_pin_configure_dt(&led1, GPIO_OUTPUT);
	gpio_pin_set_dt(&led1, 0);

	gpio_pin_configure_dt(&led2, GPIO_OUTPUT);
	gpio_pin_set_dt(&led2, 0);

	for (;;) {
		if (mcuboot_status == MCUBOOT_STATUS_UPGRADING) {
			gpio_pin_set_dt(&led0, 1);
			k_sleep(K_MSEC(200));
			gpio_pin_set_dt(&led0, 0);
			k_sleep(K_MSEC(200));
			gpio_pin_set_dt(&led1, 1);
			k_sleep(K_MSEC(200));
			gpio_pin_set_dt(&led1, 0);
			k_sleep(K_MSEC(200));
			gpio_pin_set_dt(&led2, 1);
			k_sleep(K_MSEC(200));
			gpio_pin_set_dt(&led2, 0);
			k_sleep(K_MSEC(200));
		} else {
			gpio_pin_set_dt(&led0, 0);
			gpio_pin_set_dt(&led1, 0);
			gpio_pin_set_dt(&led2, 0);
			k_sleep(K_MSEC(200));
		}
	}
}

K_THREAD_DEFINE(status_task_tid, STATUS_TASK_STACK_SIZE, dispatcher_thread, NULL, NULL, NULL,
		K_HIGHEST_APPLICATION_THREAD_PRIO, 0, 0);

void mcuboot_status_change(mcuboot_status_type_t status)
{
	mcuboot_status = status;

	if (status != MCUBOOT_STATUS_UPGRADING) {
		gpio_pin_set_dt(&led0, 0);
		gpio_pin_set_dt(&led1, 0);
		gpio_pin_set_dt(&led2, 0);
	}
}
