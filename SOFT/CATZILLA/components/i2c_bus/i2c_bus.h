#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "driver/i2c.h"

// ================= НАЛАШТУВАННЯ ШИНИ I2C =================
#define I2C_MASTER_NUM        I2C_NUM_0   
#define I2C_MASTER_SDA_IO     GPIO_NUM_6  
#define I2C_MASTER_SCL_IO     GPIO_NUM_7  
#define I2C_MASTER_FREQ_HZ    400000      // 400 кГц (Fast Mode)

// ================= АДРЕСА І2C EEPROM =================
#define EEPROM_I2C_ADDR       0x50        // 24LC128 (A0=A1=A2=GND)

// ================= КАРТА ПАМ'ЯТІ 24LC128 =================
#define EEPROM_VOL_MASTER     0x0000
#define EEPROM_VOL_FL         0x0001
#define EEPROM_VOL_FR         0x0002
#define EEPROM_VOL_CEN        0x0003
#define EEPROM_VOL_SL         0x0004
#define EEPROM_VOL_SR         0x0005
#define EEPROM_VOL_BASS       0x0006

#define EEPROM_ADDR_FILTER    0x0007      
#define EEPROM_ADDR_EQ        0x0008      

#define EEPROM_NEEDLE_TIME    0x0010      
#define EEPROM_TOTAL_TIME     0x0014      

esp_err_t i2c_master_init(void);

#endif // I2C_BUS_H