#ifndef SSD1306_H_
#define SSD1306_H_

#include "fonts.h"
#include "stm32f1xx_hal.h"

#define SSD1306_I2C_ADDR_PRIMARY 0x3C
#define SSD1306_I2C_ADDR_SECONDARY 0x3D
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

uint8_t SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_Clear(void);
void SSD1306_Display(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_WriteChar(char ch, FontDef_t *Font, uint8_t color);
void SSD1306_WriteString(char *str, FontDef_t *Font, uint8_t color);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
uint8_t SSD1306_Test_Connection(I2C_HandleTypeDef *hi2c);

#endif /* SSD1306_H_ */
