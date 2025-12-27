#include "ssd1306.h"

static I2C_HandleTypeDef *ssd1306_i2c;
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static uint8_t cursor_x = 0, cursor_y = 0;
static uint8_t ssd1306_i2c_addr = SSD1306_I2C_ADDR_PRIMARY;

HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    return HAL_I2C_Master_Transmit(ssd1306_i2c, ssd1306_i2c_addr << 1, data, 2, 1000);
}

uint8_t SSD1306_Test_Connection(I2C_HandleTypeDef *hi2c) {
    // Test primary address
    if (HAL_I2C_IsDeviceReady(hi2c, SSD1306_I2C_ADDR_PRIMARY << 1, 3, 1000) == HAL_OK) {
        ssd1306_i2c_addr = SSD1306_I2C_ADDR_PRIMARY;
        return 1;
    }
    // Test secondary address
    if (HAL_I2C_IsDeviceReady(hi2c, SSD1306_I2C_ADDR_SECONDARY << 1, 3, 1000) == HAL_OK) {
        ssd1306_i2c_addr = SSD1306_I2C_ADDR_SECONDARY;
        return 1;
    }
    return 0;
}

uint8_t SSD1306_Init(I2C_HandleTypeDef *hi2c) {
    ssd1306_i2c = hi2c;
    HAL_Delay(100);
    
    // Test connection and find correct address
    if (!SSD1306_Test_Connection(hi2c)) {
        return 0; // Connection failed
    }
    
    // Initialize display
    SSD1306_WriteCommand(0xAE); // Display OFF
    SSD1306_WriteCommand(0xD5); SSD1306_WriteCommand(0x80); // Clock
    SSD1306_WriteCommand(0xA8); SSD1306_WriteCommand(0x3F); // Multiplex
    SSD1306_WriteCommand(0xD3); SSD1306_WriteCommand(0x00); // Offset
    SSD1306_WriteCommand(0x40); // Start line
    SSD1306_WriteCommand(0x8D); SSD1306_WriteCommand(0x14); // Charge pump
    SSD1306_WriteCommand(0x20); SSD1306_WriteCommand(0x00); // Memory mode
    SSD1306_WriteCommand(0xA1); // Seg remap
    SSD1306_WriteCommand(0xC8); // Com scan dir
    SSD1306_WriteCommand(0xDA); SSD1306_WriteCommand(0x12); // Com pins
    SSD1306_WriteCommand(0x81); SSD1306_WriteCommand(0xCF); // Contrast
    SSD1306_WriteCommand(0xD9); SSD1306_WriteCommand(0xF1); // Precharge
    SSD1306_WriteCommand(0xDB); SSD1306_WriteCommand(0x40); // Vcomh
    SSD1306_WriteCommand(0xA4); // Display resume
    SSD1306_WriteCommand(0xA6); // Normal display
    SSD1306_WriteCommand(0xAF); // Display ON
    SSD1306_Clear();
    SSD1306_Display();
    return 1;
}

void SSD1306_Clear(void) {
    for (uint16_t i = 0; i < sizeof(SSD1306_Buffer); i++) {
        SSD1306_Buffer[i] = 0x00;
    }
}

void SSD1306_Display(void) {
    SSD1306_WriteCommand(0x21); SSD1306_WriteCommand(0); SSD1306_WriteCommand(SSD1306_WIDTH - 1);
    SSD1306_WriteCommand(0x22); SSD1306_WriteCommand(0); SSD1306_WriteCommand(SSD1306_HEIGHT / 8 - 1);
    for (uint16_t i = 0; i < sizeof(SSD1306_Buffer); i += 16) {
        uint8_t data[17] = {0x40};
        for (uint8_t j = 0; j < 16; j++) {
            data[j + 1] = SSD1306_Buffer[i + j];
        }
        HAL_I2C_Master_Transmit(ssd1306_i2c, ssd1306_i2c_addr << 1, data, 17, 1000);
    }
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    if (color) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
    } else {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

void SSD1306_SetCursor(uint8_t x, uint8_t y) {
    cursor_x = x;
    cursor_y = y;
}

void SSD1306_WriteChar(char ch, FontDef_t *Font, uint8_t color) {
    uint8_t b;
    uint16_t char_offset;
    
    // Ensure character is in valid range (space to Z)
    if (ch < 32 || ch > 90) {
        ch = 32; // Replace invalid characters with space
    }
    
    // Calculate character offset in font data (each character is FontHeight bytes)
    char_offset = (ch - 32) * Font->FontHeight;
    
    for (uint8_t i = 0; i < Font->FontHeight; i++) {
        b = Font->data[char_offset + i];
        for (uint8_t j = 0; j < Font->FontWidth; j++) {
            if ((b >> j) & 0x01) {
                SSD1306_DrawPixel(cursor_x + j, cursor_y + i, color);
            } else {
                SSD1306_DrawPixel(cursor_x + j, cursor_y + i, !color);
            }
        }
    }
    cursor_x += Font->FontWidth;
}

void SSD1306_WriteString(char *str, FontDef_t *Font, uint8_t color) {
    while (*str) {
        SSD1306_WriteChar(*str, Font, color);
        str++;
    }
}
