#include <csp/csp.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

#define SLEEP_TIME_MS ( 1000 )

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_ALIAS(kissusart)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* These three functions must be provided in arch specific way */
void router_start(void);
void server_start(void);
void client_start(void);


static int tx_pos = 0;
static int tx_len = 0;
static char* tx_buf_ptr;

void uart_irq_write(char* buf, int size)
{
    tx_pos = 0;
    tx_len = size;
    tx_buf_ptr = buf;

    uart_irq_tx_enable(uart_dev);
}


void serial_cb(const struct device *dev, void *user_data)
{
    static int i = 0;
    i++;

    int ret;
    if(!uart_irq_update(uart_dev)){
        return;
    }

    /* RX */
    ret = uart_irq_rx_ready(uart_dev);
    if(ret)
    {
        uint8_t c;
        uart_fifo_read(uart_dev, &c, 1);
        printk("%c", c);

        if(c == 'i')
        {
            printk("i = %d\n", i);
        }
    }

    /* TX */
    ret = uart_irq_tx_ready(uart_dev);
    if(ret)
    {
        if(tx_pos < tx_len)
        {
            ret = uart_fifo_fill(uart_dev, tx_buf_ptr+tx_pos, 1);
            tx_pos += ret;
        }
        else
        {
            ret = uart_irq_tx_complete(uart_dev);
            if(ret){
                uart_irq_tx_disable(uart_dev);
            }
        }
    }
    else
    {
        return;
    }

}

void main(void)
{
    int ret;
    char* text = "1234567890123456789012345678901234567890\r\n";

    if (!gpio_is_ready_dt(&led))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return;
    }

    if (!device_is_ready(uart_dev))
    {
        printk("UART device not found!");
        return;
    }

    ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    if (ret < 0) {
        if (ret == -ENOTSUP) {
            printk("Interrupt-driven UART API support not enabled\n");
        } else if (ret == -ENOSYS) {
            printk("UART device does not support interrupt-driven API\n");
        } else {
            printk("Error setting UART callback: %d\n", ret);
        }
        return;
    }
    uart_irq_rx_enable(uart_dev);

    /*** Cubesat Space Protocol ***/
	csp_print("Initialising CSP");

	/* Init CSP */
	csp_init();

	/* Start router */
	router_start();


    while (1)
    {
        // uart_irq_write(text, strlen(text));
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0)
        {
            return;
        }
        k_msleep(SLEEP_TIME_MS);
    }
}