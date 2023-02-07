#include <csp/csp.h>
#include <csp/drivers/usart.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <string.h>

#define SLEEP_TIME_MS ( 1000 )

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#define CPU_TEMP_NODE DT_ALIAS(cputemp)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const cpu_temp = DEVICE_DT_GET(CPU_TEMP_NODE);

/* These three functions must be provided in arch specific way */
void router_start(void);
void server_start(void);
void client_start(void);

void main(void)
{
    int ret;

    /*** User LED ***/
    if (!gpio_is_ready_dt(&led))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return;
    }

    /*** CPU temperature sensor ***/
    if (!device_is_ready(cpu_temp))
    {
        printk("sensor: device %s not ready.\n", cpu_temp->name);
        return;
    }

	ret = sensor_sample_fetch(cpu_temp);
	if (ret) {
		printk("Failed to fetch sample (%d)\n", ret);
	}

	struct sensor_value val;
	ret = sensor_channel_get(cpu_temp, SENSOR_CHAN_DIE_TEMP, &val);
	if (ret) {
		printk("Failed to get data (%d)\n", ret);
	}

	printk("CPU Die temperature[%s]: %.1f °C (%d %d)\n", cpu_temp->name, sensor_value_to_double(&val), val.val1, val.val2);

    /*** Cubesat Space Protocol ***/
    printk("Initializing CSP\n");

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();

    /* Add interface(s) */
    csp_iface_t * default_iface = NULL;

    // Configure USART in devicetree and here pass NULL
    int error = csp_usart_open_and_add_kiss_interface(NULL, CSP_IF_KISS_DEFAULT_NAME, &default_iface);
    if (error != CSP_ERR_NONE)
    {
        csp_print("Failed to add KISS interface [%s], error: %d\n", DEVICE_DT_NAME(DT_ALIAS(cspkissuart)), error);
        return;
    }
    else
    {
        csp_print("KISS interface OK\n");
        default_iface->addr = 1;
    }

    csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS); 

    csp_print("Connection table\r\n");
    csp_conn_print_table();

    csp_print("Interfaces\r\n");
    csp_iflist_print();

    csp_print("Route table\r\n");
    csp_rtable_print();

    server_start();

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