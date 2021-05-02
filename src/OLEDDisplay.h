#ifndef OLEDDisplay_h
#define OLEDDisplay_h

#if defined(TTGO_T_EIGHT) || defined(SH1106WIRE)
#include <SH1106Wire.h>
#define OLED_I2C_ADDRESS  0x3c
#define OLED_I2C_SDA      21
#define OLED_I2C_SCL      22
SH1106Wire                display(OLED_I2C_ADDRESS, OLED_I2C_SDA, OLED_I2C_SCL);
#endif
#if defined(SSD1306WIRE)
#include <SSD1306Wire.h>
#define OLED_I2C_ADDRESS  0x3c
#define OLED_I2C_SDA      SDA
#define OLED_I2C_SCL      SCL
SSD1306Wire               display(OLED_I2C_ADDRESS, OLED_I2C_SDA, OLED_I2C_SCL);
#endif



#endif //OLEDDisplay_h