#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/drivers/usart.h>
#include "i2c.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <string.h>

#define OBC_ADDR ( 1 )
#define EPS_ADDR ( 2 )
#define COM_ADDR ( 3 )

#define SLEEP_TIME_MS ( 1000 )

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
// #define CSP_I2C_NODE DT_ALIAS(cspi2c)

#define ADDR0_NODE	DT_ALIAS(addr0)
#define ADDR1_NODE	DT_ALIAS(addr1)

static const struct gpio_dt_spec addr0 = GPIO_DT_SPEC_GET_OR(ADDR0_NODE, gpios, {0});
static const struct gpio_dt_spec addr1 = GPIO_DT_SPEC_GET_OR(ADDR1_NODE, gpios, {0});
                                  
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* These three functions must be provided in arch specific way */
void router_start(void);
void server_start(void);
void client_start(void);

void who_am_i(uint8_t addr)
{
    switch(addr)
    {
    case OBC_ADDR:
        printk("HW: OBC\n");
        break;
    case EPS_ADDR:
        printk("HW: EPS\n");
        break;
    case COM_ADDR:
        printk("HW: COM\n");
        break;
    default:
        printk("HW: error!\n");
        break;
    }
}

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

    /*** HW address input ***/
    if (!gpio_is_ready_dt(&addr0))
    {
        printk("Error: HW address device %s is not ready\n", addr0.port->name);
        return;
    }
    if (!gpio_is_ready_dt(&addr1))
    {
        printk("Error: HW address device %s is not ready\n", addr1.port->name);
        return;
    }

    ret = gpio_pin_configure_dt(&addr0, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure %s pin %d\n", ret, addr0.port->name, addr0.pin);
        return;
    }
    ret = gpio_pin_configure_dt(&addr1, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure %s pin %d\n", ret, addr1.port->name, addr1.pin);
        return;
    }

    uint8_t my_addr = 0;
    if(gpio_pin_get_dt(&addr0)) { my_addr += 1; }
    if(gpio_pin_get_dt(&addr1)) { my_addr += 2; }

    who_am_i(my_addr);

    /*** Cubesat Space Protocol ***/
    printk("Initializing CSP\n");

    /* Init CSP */
    csp_init();
    csp_dbg_packet_print = 1;

    /* Start router */
    router_start();

    /* Add interface(s) */
    csp_iface_t * kiss_iface = NULL;
    csp_iface_t * i2c_iface = NULL;

    // Configure USART in devicetree and here pass NULL
    if(my_addr == COM_ADDR)
    {
        ret = csp_usart_open_and_add_kiss_interface(NULL, CSP_IF_KISS_DEFAULT_NAME, &kiss_iface);
        if (ret != CSP_ERR_NONE)
        {
            csp_print("Failed to add KISS interface [%s], error: %d\n", DEVICE_DT_NAME(DT_ALIAS(cspkissuart)), ret);
            return;
        }
        else
        {
            csp_print("KISS interface OK\n");
            kiss_iface->addr = my_addr;
        }

        csp_rtable_set(32, 9, kiss_iface, CSP_NO_VIA_ADDRESS);
    }

    // Configure I2C
    csp_i2c_conf_t conf = { .address = my_addr };
    ret = csp_i2c_open_and_add_interface(&conf, CSP_IF_I2C_DEFAULT_NAME, &i2c_iface);
    if (ret != CSP_ERR_NONE)
    {
        csp_print("Failed to add I2C interface [%s], error: %d\n", DEVICE_DT_NAME(DT_ALIAS(cspi2c)), ret);
        return;
    }
    else
    {
        csp_print("I2C interface OK\n");
    }

    csp_rtable_set(0, 9, i2c_iface, CSP_NO_VIA_ADDRESS);
    if(my_addr != COM_ADDR)
    {
        csp_rtable_set(32, 9, i2c_iface, COM_ADDR);
    }

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