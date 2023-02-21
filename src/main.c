#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include "i2c.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <string.h>

#define SLEEP_TIME_MS ( 1000 )

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
// #define CSP_I2C_NODE DT_ALIAS(cspi2c)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
// static const struct i2c_dt_spec i2c = I2C_DT_SPEC_GET(CSP_I2C_NODE);

/* These three functions must be provided in arch specific way */
void router_start(void);
void server_start(void);
void client_start(void);

void main(void)
{
    int ret;

    const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

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

    /*** I2C ***/
    // if (!device_is_ready(i2c_dev))
    // {
    //     printk("I2C not ready!\n");
    //     return;
    // }

    // ret = csp_i2c_open();
    // if (ret != CSP_ERR_NONE)
    // {
    //     csp_print("Failed to open I2C , error: %d\n", ret);
    //     return;
    // }


    /*** Cubesat Space Protocol ***/
    printk("Initializing CSP\n");

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();

    /* Add interface(s) */
    csp_iface_t * kiss_iface = NULL;
    csp_iface_t * i2c_iface = NULL;

    // Configure USART in devicetree and here pass NULL
    ret = csp_usart_open_and_add_kiss_interface(NULL, CSP_IF_KISS_DEFAULT_NAME, &kiss_iface);
    if (ret != CSP_ERR_NONE)
    {
        csp_print("Failed to add KISS interface [%s], error: %d\n", DEVICE_DT_NAME(DT_ALIAS(cspkissuart)), ret);
        return;
    }
    else
    {
        csp_print("KISS interface OK\n");
        kiss_iface->addr = 1;
    }

    // Configure I2C
    ret = csp_i2c_open_and_add_interface(NULL, CSP_IF_I2C_DEFAULT_NAME, &i2c_iface);
    if (ret != CSP_ERR_NONE)
    {
        csp_print("Failed to add I2C interface [%s], error: %d\n", DEVICE_DT_NAME(DT_ALIAS(cspi2c)), ret);
        return;
    }
    else
    {
        csp_print("I2C interface OK\n");
        i2c_iface->addr = 1;
    }


    csp_rtable_set(0, 0, kiss_iface, CSP_NO_VIA_ADDRESS);

    csp_print("Connection table\r\n");
    csp_conn_print_table();

    csp_print("Interfaces\r\n");
    csp_iflist_print();

    csp_print("Route table\r\n");
    csp_rtable_print();

    server_start();

    uint8_t buf[16] = { "HelloI2Cmessage!" };

    while (1)
    {
        // ret = i2c_write(i2c_dev, buf, 16, 11);
        // printk("i2c_write: %d\n", ret);

        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0)
        {
            return;
        }
        k_msleep(SLEEP_TIME_MS);
    }
}