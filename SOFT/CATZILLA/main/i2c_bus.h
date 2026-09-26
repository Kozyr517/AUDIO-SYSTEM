#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "driver/i2c_master.h"
#include "esp_err.h"

// Піни I2C 
#define I2C_MASTER_SCL_IO    6
#define I2C_MASTER_SDA_IO    7

// Глобальний хендл шини I2C
extern i2c_master_bus_handle_t i2c_bus_handle;

/**
 * @brief Ініціалізація шини I2C Master
 * @return esp_err_t ESP_OK у разі успіху
 */
esp_err_t i2c_bus_init(void);

#endif // I2C_BUS_H