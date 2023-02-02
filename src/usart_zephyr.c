#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/drivers/usart.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <stdlib.h>

#define RX_THREAD_STACK_SIZE ( 800 )
#define RX_THREAD_PRIORITY ( 0 )

#define RX_BUFFER_SIZE ( 200 )
#define RX_RETRY ( 250 )

K_THREAD_STACK_DEFINE(rx_thread_stack_area, RX_THREAD_STACK_SIZE);

static const struct device *const csp_kiss_uart_dev = DEVICE_DT_GET(DT_ALIAS(cspkissuart));

typedef struct {
    csp_usart_callback_t rx_callback;
    void * user_data;
    csp_usart_fd_t fd; // TODO: For Zephyr change underlying type to struct device pointer?
    struct k_thread rx_thread;
    k_tid_t rx_thread_id;
} usart_context_t;

K_MUTEX_DEFINE(lock);

void csp_usart_lock(void * driver_data)
{
    k_mutex_lock(&lock, K_NO_WAIT);
}

void csp_usart_unlock(void * driver_data)
{
    k_mutex_unlock(&lock);
}

static void usart_rx_thread(void * p1, void * p2, void * p3)
{
	// ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

    usart_context_t * ctx = p1;
    int ret;
    uint8_t c;
    int length = 0;
    char rx_buf[RX_BUFFER_SIZE] = { 0 };
    uint8_t retry = 0;

    while(1)
    {
        ret = uart_poll_in(csp_kiss_uart_dev, rx_buf+length);

        if(ret == 0 && length < RX_BUFFER_SIZE)
        {
            // csp_print("%c", c);

            retry = 0;
            length++;
        }
        else if(ret == -1)
        {
            retry++;
            if(retry >= RX_RETRY)
            {
                if(length > 0)
                {
                    csp_print("Len: %d, buf: %s\n", length, rx_buf);
                    ctx->rx_callback(ctx->user_data, rx_buf, length, NULL);

                    length = 0;
                    k_msleep(10);
                }
                else
                {
                    // csp_print("Len: %d\n", length);
                }
                retry = 0;

                // k_msleep(10);
            }
        }
    }

    // usart_context_t * ctx = arg;
    // const unsigned int CBUF_SIZE = 400;
    // uint8_t * cbuf = malloc(CBUF_SIZE);

    // // Receive loop
    // while (1) {
    //     int length = read(ctx->fd, cbuf, CBUF_SIZE);
    //     if (length <= 0) {
    //         csp_print("%s: read() failed, returned: %d\n", __FUNCTION__, length);
    //         exit(1);
    //     }
    //     ctx->rx_callback(ctx->user_data, cbuf, length, NULL);
    // }
}

int csp_usart_write(csp_usart_fd_t fd, const void * data, size_t data_length)
{
    // if (fd >= 0) {
    // 	int res = write(fd, data, data_length);
    // 	if (res >= 0) {
    // 		return res;
    // 	}
    // }
    return CSP_ERR_TX;  // best matching CSP error code.
}

int csp_usart_open(const csp_usart_conf_t * conf, csp_usart_callback_t rx_callback, void * user_data, csp_usart_fd_t * return_fd)
{
    // USART parameters configured in devicetree, no use here
    ARG_UNUSED(conf);

    if (!device_is_ready(csp_kiss_uart_dev))
    {
        csp_print("UART KISS device not found!");
        return CSP_ERR_DRIVER;
    }

    // Configure polling/interrupts/events here
    // For now do nothing, but keep the commented code here

    // ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    // if (ret < 0) {
    //     if (ret == -ENOTSUP) {
    //         printk("Interrupt-driven UART API support not enabled\n");
    //     } else if (ret == -ENOSYS) {
    //         printk("UART device does not support interrupt-driven API\n");
    //     } else {
    //         printk("Error setting UART callback: %d\n", ret);
    //     }
    //     return;
    // }
    // uart_irq_rx_enable(uart_dev);

    // Allocate context memory
    usart_context_t * ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        csp_print("%s: Error allocating context, device: [%s], errno: %s\n", __FUNCTION__, conf->device, strerror(errno));
        return CSP_ERR_NOMEM;
    }
    ctx->rx_callback = rx_callback;
    ctx->user_data = user_data;
    // ctx->fd = fd; // TODO: Read struct member comment

    // Spawn RX thread
    if (rx_callback)
    {
        // TODO: How about creating thread at compile time and only enabling it here?
        ctx->rx_thread_id = k_thread_create(&ctx->rx_thread, rx_thread_stack_area,
            K_THREAD_STACK_SIZEOF(rx_thread_stack_area), usart_rx_thread,
            ctx, NULL, NULL,
            RX_THREAD_PRIORITY, 0, K_NO_WAIT);

        // if(is there anything that can go wrong?)
        // {
        //     csp_print("%s: k_thread_create() failed to create Rx thread for device: [%s], errno: %s\n", __FUNCTION__, conf->device, strerror(errno));
        //     free(ctx);
        //     return CSP_ERR_NOMEM;
        // }
    }

    if (return_fd) {
        // *return_fd = fd; // TODO: Read struct member comment
    }

    return CSP_ERR_NONE;
}
