#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/drivers/usart.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <stdlib.h>

// TODO: Set buffer size using KISS interface MTU value
#define RX_BUFFER_SIZE ( 200 )
#define RX_TIMEOUT_US ( 50000 )

static const struct device *const csp_kiss_uart_dev = DEVICE_DT_GET(DT_ALIAS(cspkissuart));

static uint8_t rx_buffer[RX_BUFFER_SIZE];

typedef struct {
    csp_usart_callback_t rx_callback;
    void * user_data;
    csp_usart_fd_t fd; // TODO: For Zephyr change underlying type to struct device pointer?
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

int csp_usart_write(csp_usart_fd_t fd, const void * data, size_t data_length)
{
    // Timeout in microseconds valid only if flow control is enabled
    csp_print("csp_usart_write: %d\n", data_length);
    int ret = uart_tx(csp_kiss_uart_dev, data, data_length, SYS_FOREVER_US);
    if(ret != 0)
    {
        return ret;
    }

    return data_length;
}

void csp_kiss_uart_event_cb(const struct device* dev, struct uart_event* evt, void* user_data)
{
    csp_print("uart event: %d\n", evt->type);
    usart_context_t * ctx = user_data;

    switch(evt->type)
    {
    case UART_TX_DONE:
        csp_print("tx: %d\n", evt->data.tx.len);
        break;

    case UART_TX_ABORTED:
        break;

	case UART_RX_RDY:
        uart_rx_disable(csp_kiss_uart_dev);
        csp_print("rx: %d\n", evt->data.rx.len);
        ctx->rx_callback(ctx->user_data, evt->data.rx.buf+evt->data.rx.offset, evt->data.rx.len, NULL);
        break;

    case UART_RX_BUF_REQUEST:
        break;

	case UART_RX_BUF_RELEASED:
        break;

	case UART_RX_DISABLED:
        uart_rx_enable(csp_kiss_uart_dev, rx_buffer, RX_BUFFER_SIZE, RX_TIMEOUT_US);
        break;

	case UART_RX_STOPPED:
        break;

    }
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

    // Allocate context memory
    usart_context_t * ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        csp_print("%s: Error allocating context, device: [%s], errno: %s\n", __FUNCTION__, csp_kiss_uart_dev->name, strerror(errno));
        return CSP_ERR_NOMEM;
    }
    ctx->rx_callback = rx_callback;
    ctx->user_data = user_data;
    // ctx->fd = fd; // TODO: Read struct member comment

    // Setup UART callback
    int ret = uart_callback_set(csp_kiss_uart_dev, csp_kiss_uart_event_cb, ctx);
    if(ret != 0)
    {
        csp_print("Error setting UART callback, device: [%s], return: %d\n", csp_kiss_uart_dev->name, ret);

        return CSP_ERR_DRIVER;
    }

    // Enable RX if callback provided
    if (rx_callback)
    {
        ret = uart_rx_enable(csp_kiss_uart_dev, rx_buffer, RX_BUFFER_SIZE, RX_TIMEOUT_US);
        if(ret != 0)
        {
            csp_print("Error enabling UART RX, device: [%s], return: %d (%s)\n", csp_kiss_uart_dev->name, ret, strerror(-ret));

            return CSP_ERR_DRIVER;
        }
    }

    if (return_fd)
    {
        // *return_fd = fd; // TODO: Read struct member comment
    }

    return CSP_ERR_NONE;
}
