#pragma once

/**
   @file

   I2C driver.
*/

#include <csp/interfaces/csp_if_i2c.h>


/**
   I2C configuration
   @see csp_i2c_open()
*/
typedef struct csp_i2c_conf {
    //! Bus bitrate
    uint32_t bitrate;
    //! Slave address
    uint8_t address;
} csp_i2c_conf_t;

// /** 
//    Callback for returning data to application
// 
//    @param[in] buf data received
//    @param[in] len data length (number of bytes in \a buf).
//    @param[out] pxTaskWoken Valid reference if called from ISR, otherwise NULL!
// */
// typedef void (*csp_i2c_callback_t) (void * user_data, uint8_t *buf, size_t len, void *pxTaskWoken);

/**
 * Write data on open I2C.
 * @return number of bytes written on success, a negative value on failure. 
 */
int csp_i2c_write(void * driver_data, csp_packet_t * packet);

/**
 * Lock the device, so only a single user can use I2C device at a time
 */
// void csp_i2c_lock(void * driver_data);
void csp_i2c_lock(void * driver_data, void * pxTaskWoken);

/**
 * Unlock the I2C device again
 */
void csp_i2c_unlock(void * driver_data);

/**
   Open an I2C device and add interface

   @param[in] conf I2C configuration.
   @param[in] ifname interface name (will be copied), or use NULL for default name.
   @param[out] return_iface the added interface.
   @return #CSP_ERR_NONE on success, otherwise an error code.
*/
int csp_i2c_open_and_add_interface(const csp_i2c_conf_t *conf, const char * ifname, csp_iface_t ** return_iface);
