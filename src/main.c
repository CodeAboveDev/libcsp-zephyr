#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS ( 250 )

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return;
    }

    while (1)
    {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0)
        {
            return;
        }
        k_msleep(SLEEP_TIME_MS);
    }
}