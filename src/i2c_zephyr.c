#include "i2c.h"

#include <csp/csp.h>
#include <csp/csp_id.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#include <stdlib.h>

typedef struct
{
    char name[CSP_IFLIST_NAME_MAX + 1];
    csp_iface_t iface;
    csp_i2c_interface_data_t ifdata;
} i2c_context_t;

int csp_i2c_target_write_requested_cb(struct i2c_target_config *config);
int csp_i2c_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val);
int csp_i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val);
int csp_i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val);
int csp_i2c_target_stop_cb(struct i2c_target_config *config);

static const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

static csp_packet_t* packet = NULL;
static i2c_context_t* ctx = NULL;
volatile static uint16_t rx_length = 0;

static struct i2c_target_callbacks csp_target_cbs = 
{
    .read_processed = csp_i2c_target_read_processed_cb,
    .read_requested = csp_i2c_target_read_requested_cb,
    .stop = csp_i2c_target_stop_cb,
    .write_received = csp_i2c_target_write_received_cb,
    .write_requested = csp_i2c_target_write_requested_cb
};

static struct i2c_target_config csp_i2c_target_config =
{
    .address = 0,
    .flags = 0,
    .callbacks = &csp_target_cbs
};

int csp_i2c_target_write_requested_cb(struct i2c_target_config *config)
{
    // TODO: IS THIS CALLED IN ISR?
    // TODO: yes, this is ISR context, handle properly!

    // TODO: Get I2C ctx by i2c_target_config

    // There is an incoming packet, let's prepare a buffer for it
    // TODO: Get buffer size from interface MTU size
    // TODO: Keep packet in i2c ctx

    packet = csp_buffer_get_isr(CSP_BUFFER_SIZE);
    if (packet == NULL)
    {
        // No more memory, return negative error code to reject incoming write request
        return -1;
    }

    csp_id_setup_rx(packet);
    rx_length = 0;

    return 0;
}

int csp_i2c_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val)
{
    // Only write is support, return negative error code to reject incoming write request
    return -ENOTSUP;
}

int csp_i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
    if (packet == NULL)
    {
        // Buffer not prepared, abort receiving
        csp_print("Write RX packet NULL!\n");
        return -CSP_ERR_NOBUFS;
    }
    packet->frame_begin[rx_length++] = val;

    return 0;
}

int csp_i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val)
{
    // Only write is support, return negative error code to reject incoming write request
    return -ENOTSUP;
}

int csp_i2c_target_stop_cb(struct i2c_target_config *config)
{
    csp_print("csp_i2c_target_stop_cb, rx_len: %d\n", rx_length);

    uint8_t task_woken;

    packet->frame_length = rx_length;
    csp_i2c_rx(&ctx->iface, packet, &task_woken);

    return 0;
}

int csp_i2c_open(void)
{
    int ret = i2c_target_register(i2c_dev, &csp_i2c_target_config);
    if(ret != 0)
    {
        csp_print("i2c_target_register() failed, error: %d\n", ret);
        return CSP_ERR_DRIVER;
    }

    return CSP_ERR_NONE;
}

#define WRITE_TRY ( 3 ) 
int csp_i2c_write(void * driver_data, csp_packet_t * packet)
{
    int ret;
    csp_print("%s[%s]: sending packet, size: %d, dst: %d, cfpid: %d\n", __FUNCTION__, ctx->name, packet->frame_length, packet->id.dst, packet->cfpid);

    // // csp_i2c_tx() sets cfpid to either via address or destination address
    // ret = i2c_write(i2c_dev, packet->frame_begin, packet->frame_length, packet->cfpid);
    // csp_print("i2c_write: %d\n", ret);

    struct i2c_msg msg;

    /* Setup I2C messages */
    if (!device_is_ready(i2c_dev))
    {
        csp_print("i2c not ready\n");
        return CSP_ERR_TX;
    }
    csp_print("i2c ready\n");

    /* Data to be written, and STOP after this. */
    msg.buf = packet->frame_begin;
    msg.len = packet->frame_length;
    msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

    uint8_t try = 0;
    do
    {
        ret = i2c_transfer(i2c_dev, &msg, 1, packet->cfpid);
        csp_print("i2c_transfer: %d\n", ret);
        try++;
    } while ((ret != 0) && (try < WRITE_TRY));

    if(ret != 0)
    {
        return CSP_ERR_TX;
    }

    return packet->frame_length;
}

int csp_i2c_open_and_add_interface(const csp_i2c_conf_t *conf, const char * ifname, csp_iface_t ** return_iface)
{
    if (ifname == NULL) {
        ifname = CSP_IF_I2C_DEFAULT_NAME;
    }

    // TODO: How to store other than global?
    // i2c_context_t* ctx = calloc(1, sizeof(*ctx));
    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        csp_print("%s: Error allocating context, device: [%s], errno: %s\n", __FUNCTION__, i2c_dev->name, strerror(errno));
        return CSP_ERR_NOMEM;
    }

    strncpy(ctx->name, ifname, sizeof(ctx->name) - 1);
    ctx->iface.name = ctx->name;
    ctx->iface.addr = conf->address;
    ctx->iface.driver_data = ctx;
    ctx->iface.interface_data = &ctx->ifdata;
    ctx->ifdata.tx_func = csp_i2c_write;

    if (!device_is_ready(i2c_dev))
    {
        csp_print("%s[%s]: I2C not ready!\n", __FUNCTION__, ctx->name);
        return CSP_ERR_DRIVER;
    }

    csp_i2c_target_config.address = conf->address;
    int ret = i2c_target_register(i2c_dev, &csp_i2c_target_config);
    if(ret != 0)
    {
        csp_print("%s[%s]: i2c_target_register() failed, error: %d\n", __FUNCTION__, ctx->name, ret);
        free(ctx);
        return CSP_ERR_DRIVER;
    }

    /* Add interface to CSP */
    int res = csp_i2c_add_interface(&ctx->iface);
    if (res != CSP_ERR_NONE) {
        csp_print("%s[%s]: csp_i2c_add_interface() failed, error: %d\n", __FUNCTION__, ctx->name, res);
        free(ctx);
        return res;
    }

    if (return_iface) {
        *return_iface = &ctx->iface;
    }

    return CSP_ERR_NONE;
}




