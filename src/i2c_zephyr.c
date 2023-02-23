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
// static const struct i2c_dt_spec csp_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(i2c1));

static csp_packet_t* packet = NULL;
static i2c_context_t* ctx = NULL;
static uint16_t rx_length = 0;

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
    .address = 11,
    .flags = 0,
    .callbacks = &csp_target_cbs
};

int csp_i2c_target_write_requested_cb(struct i2c_target_config *config)
{
    // csp_print("csp_i2c_target_write_requested_cb\n");
    // TODO: IS THIS CALLED IN ISR?
    // TODO: yes, this is ISR context, handle properly!

    // TODO: Get I2C ctx by i2c_target_config

    // TODO: Packet buffer might still be in use
    // if(packet != NULL)
    // {
    // }

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
    // csp_print("csp_i2c_target_read_requested_cb\n");

    // Only write is support, return negative error code to reject incoming write request
    return -ENOTSUP;
}

int csp_i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
    // csp_print("csp_i2c_target_write_received_cb\n");

    if (packet == NULL)
    {
        // Buffer not prepared, abort receiving
        return -1;
    }
    packet->frame_begin[rx_length++] = val;

    return 0;
}

int csp_i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val)
{
    // csp_print("csp_i2c_target_read_processed_cb\n");

    // Only write is support, return negative error code to reject incoming write request
    return -ENOTSUP;
}

int csp_i2c_target_stop_cb(struct i2c_target_config *config)
{
    // csp_print("csp_i2c_target_stop_cb\n");

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
        csp_print("i2c_target_register failed, error: %d\n", ret);
        return CSP_ERR_DRIVER;
    }

    return CSP_ERR_NONE;
}

int csp_i2c_write(void * driver_data, csp_packet_t * packet)
{
    // i2c_context_t* ctx = driver_data;

    return CSP_ERR_TX;
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
    ctx->iface.driver_data = ctx;
    ctx->iface.interface_data = &ctx->ifdata;
    ctx->ifdata.tx_func = csp_i2c_write;

    if (!device_is_ready(i2c_dev))
    {
        csp_print("%s[%s]: I2C not ready!\n", __FUNCTION__, ctx->name);
        return CSP_ERR_DRIVER;
    }

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




