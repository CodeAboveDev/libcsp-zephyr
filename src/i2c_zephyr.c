#include <csp/csp.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

int csp_i2c_target_write_requested_cb(struct i2c_target_config *config);
int csp_i2c_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val);
int csp_i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val);
int csp_i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val);
int csp_i2c_target_stop_cb(struct i2c_target_config *config);

// static struct i2c_target_config csp_i2c_target_config = {
//     .address = 0xBA,
//     .flags = 0,
// };
static const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

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
    return 0;
}

int csp_i2c_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val)
{
    csp_print("csp_i2c_target_read_requested_cb\n");

}

int csp_i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
    // csp_print("csp_i2c_target_write_received_cb\n");
    csp_print("%c", val);
    return 0;

}

int csp_i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val)
{
    csp_print("csp_i2c_target_read_processed_cb\n");

}

int csp_i2c_target_stop_cb(struct i2c_target_config *config)
{
    csp_print("\n");
    // csp_print("csp_i2c_target_stop_cb\n");
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




