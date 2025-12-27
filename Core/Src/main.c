#include "stm32f1xx_hal.h"
#include "ssd1306.h"
#include "fonts.h"
#include <string.h>
#include <stdio.h>

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;

#define DHT11_PIN GPIO_PIN_0
#define DHT11_PORT GPIOA
#define TRIG_PIN GPIO_PIN_1
#define ECHO_PIN GPIO_PIN_2
#define ECHO_PORT GPIOA
#define LED_PIN GPIO_PIN_0
#define LED_PORT GPIOB

// Keypad Configuration
// Row Pins (Outputs)
#define ROW1_PIN GPIO_PIN_3
#define ROW2_PIN GPIO_PIN_4
#define ROW3_PIN GPIO_PIN_5
#define ROW4_PIN GPIO_PIN_6 // This row is now unused for any menu key
// Column Pins (Inputs)
#define COL1_PIN GPIO_PIN_7 // Not used in menu
#define COL2_PIN GPIO_PIN_8 // Used for '5' (DHT) and '8' (Sonar)
#define COL3_PIN GPIO_PIN_9 // <--- NEW PIN FOR COLUMN 3 (Key '9' / Back)
#define KEYPAD_PORT GPIOA

typedef enum { MENU, DHT11_MODE, SONAR_MODE } State;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
uint8_t read_DHT11(void);
uint32_t read_HCSR04(void);
char read_keypad(void);
void blink_led(uint8_t times);
void fast_blink_led(void);
void delay_us(uint32_t us);
void DHT11_Start(void);
uint8_t DHT11_Check_Response(void);
uint8_t DHT11_Read(void);

uint8_t dht11_data[5] = {0}; // Initialize to zero
char display_buf[50];
State current_state = MENU;
uint8_t current_temperature = 0; // Track current temperature for LED blinking

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    
    // Start the timer for microsecond delays
    HAL_TIM_Base_Start(&htim2);
    
    // Initialize OLED with error checking
    if (!SSD1306_Init(&hi2c1)) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        while(1) {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            HAL_Delay(250);
        }
    }

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); // LED ON
    
    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("TEST", &Font_7x10, 1);
    SSD1306_Display();
    HAL_Delay(2000);
    
    // Show main menu (Keys: 5, 8, 9)
    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("5: DHT11", &Font_7x10, 1);
    SSD1306_SetCursor(0, 15);
    SSD1306_WriteString("8: Sonar", &Font_7x10, 1);
    SSD1306_SetCursor(0, 30);
    SSD1306_WriteString("9: Return", &Font_7x10, 1);
    SSD1306_Display();

    while (1) {
        char key = read_keypad();
        if (key) {
            // Check for back button (Key 9)
            if (key == '9') {
                current_state = MENU;
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
                SSD1306_Clear();
                SSD1306_SetCursor(0, 0);
                SSD1306_WriteString("5: DHT11", &Font_7x10, 1);
                SSD1306_SetCursor(0, 15);
                SSD1306_WriteString("8: Sonar", &Font_7x10, 1);
                SSD1306_SetCursor(0, 30);
                SSD1306_WriteString("9: Return", &Font_7x10, 1);
                SSD1306_Display();
                HAL_Delay(500);
            } 
            else if (current_state == MENU) {
                // Key '5' enters DHT11 mode
                if (key == '5') {
                    current_state = DHT11_MODE;
                    blink_led(2);
                    SSD1306_Clear();
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString("Reading DHT11...", &Font_7x10, 1);
                    SSD1306_Display();
                    HAL_Delay(1000);
                    
                    if (read_DHT11()) {
                        SSD1306_Clear();
                        sprintf(display_buf, "Temp: %d.%d C", dht11_data[2], dht11_data[3]);
                        SSD1306_SetCursor(0, 0);
                        SSD1306_WriteString(display_buf, &Font_7x10, 1);
                        sprintf(display_buf, "Humid: %d.%d %%", dht11_data[0], dht11_data[1]);
                        SSD1306_SetCursor(0, 15);
                        SSD1306_WriteString(display_buf, &Font_7x10, 1);
                        
                        if (dht11_data[2] > 32) {
                            SSD1306_SetCursor(0, 30);
                            SSD1306_WriteString("HIGH TEMP!", &Font_7x10, 1);
                        }
                        
                        SSD1306_SetCursor(0, 45);
                        SSD1306_WriteString("Press 9 to exit", &Font_7x10, 1);
                    } else {
                        SSD1306_Clear();
                        SSD1306_SetCursor(0, 0);
                        SSD1306_WriteString("DHT11 Error!", &Font_7x10, 1);
                        SSD1306_SetCursor(0, 15);
                        SSD1306_WriteString("Check wiring", &Font_7x10, 1);
                        SSD1306_SetCursor(0, 45);
                        SSD1306_WriteString("Press 9 to exit", &Font_7x10, 1);
                    }
                    SSD1306_Display();
                } 
                // Key '8' enters Sonar mode
                else if (key == '8') {
                    current_state = SONAR_MODE;
                    blink_led(2);
                    SSD1306_Clear();
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString("Reading Sonar...", &Font_7x10, 1);
                    SSD1306_Display();
                    HAL_Delay(500);
                    
                    uint32_t distance = read_HCSR04();
                    SSD1306_Clear();
                    if (distance == 0) {
                        SSD1306_SetCursor(0, 0);
                        SSD1306_WriteString("No object found", &Font_7x10, 1);
                    } else if (distance == 999) {
                        SSD1306_SetCursor(0, 0);
                        SSD1306_WriteString("Out of range", &Font_7x10, 1);
                        SSD1306_SetCursor(0, 15);
                        SSD1306_WriteString("(>20cm)", &Font_7x10, 1);
                    } else {
                        sprintf(display_buf, "Distance: %ld cm", distance);
                        SSD1306_SetCursor(0, 0);
                        SSD1306_WriteString(display_buf, &Font_7x10, 1);
                        
                        SSD1306_SetCursor(0, 15);
                        SSD1306_WriteString("OBJECT DETECTED!", &Font_7x10, 1);
                    }
                    SSD1306_SetCursor(0, 40);
                    SSD1306_WriteString("Press 9 to exit", &Font_7x10, 1);
                    SSD1306_Display();
                }
            }
        }
        
        // Continuous reading in sensor modes
        if (current_state == DHT11_MODE) {
            static uint32_t last_dht_read = 0;
            static uint32_t last_led_blink = 0;
            
            if (HAL_GetTick() - last_dht_read > 2000) {
                if (read_DHT11()) {
                    SSD1306_Clear();
                    sprintf(display_buf, "Temp: %d.%d C", dht11_data[2], dht11_data[3]);
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString(display_buf, &Font_7x10, 1);
                    sprintf(display_buf, "Humid: %d.%d %%", dht11_data[0], dht11_data[1]);
                    SSD1306_SetCursor(0, 15);
                    SSD1306_WriteString(display_buf, &Font_7x10, 1);
                    
                    if (dht11_data[2] > 28) {
                        SSD1306_SetCursor(0, 30);
                        SSD1306_WriteString("HIGH TEMP!", &Font_7x10, 1);
                    }
                    
                    SSD1306_SetCursor(0, 45);
                    SSD1306_WriteString("Press 9 to exit", &Font_7x10, 1);
                    SSD1306_Display();
                }
                last_dht_read = HAL_GetTick();
            }
            
            if (current_temperature > 28) {
                if (HAL_GetTick() - last_led_blink > 100) {
                    fast_blink_led();
                    last_led_blink = HAL_GetTick();
                }
            } else {
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
            }
        } else if (current_state == SONAR_MODE) {
            static uint32_t last_sonar_read = 0;
            static uint32_t last_led_blink = 0;
            static uint32_t current_distance = 0;
            
            if (HAL_GetTick() - last_sonar_read > 500) {
                current_distance = read_HCSR04();
                SSD1306_Clear();
                if (current_distance == 0) {
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString("No object found", &Font_7x10, 1);
                } else if (current_distance == 999) {
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString("Out of range", &Font_7x10, 1);
                    SSD1306_SetCursor(0, 15);
                    SSD1306_WriteString("(>20cm)", &Font_7x10, 1);
                } else {
                    sprintf(display_buf, "Distance: %ld cm", current_distance);
                    SSD1306_SetCursor(0, 0);
                    SSD1306_WriteString(display_buf, &Font_7x10, 1);
                    
                    SSD1306_SetCursor(0, 15);
                    SSD1306_WriteString("OBJECT DETECTED!", &Font_7x10, 1);
                }
                SSD1306_SetCursor(0, 40);
                SSD1306_WriteString("Press 9 to exit", &Font_7x10, 1)	;
                SSD1306_Display();
                last_sonar_read = HAL_GetTick();
            }
            
            if (current_distance > 0 && current_distance < 999) {
                if (HAL_GetTick() - last_led_blink > 100) {
                    fast_blink_led();
                    last_led_blink = HAL_GetTick();
                }
            } else {
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
            }
        }
        
        HAL_Delay(50);
    }
}

void SystemClock_Config(void) {
    // ... (SystemClock_Config function remains the same)
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // LED (PB0)
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    // DHT11 (PA0)
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

    // HC-SR04 (PA1: Trig, PA2: Echo)
    GPIO_InitStruct.Pin = TRIG_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = ECHO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Keypad Output Pins (Rows)
    // Initializing ROW1, ROW2, ROW3, and ROW4 (PA3, PA4, PA5, PA6)
    GPIO_InitStruct.Pin = ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(KEYPAD_PORT, &GPIO_InitStruct);

    // Keypad Input Pins (Columns)
    // Including the new COL3_PIN (PA9)
    GPIO_InitStruct.Pin = COL1_PIN | COL2_PIN | COL3_PIN; // PA7, PA8, PA9 <--- CHANGED
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEYPAD_PORT, &GPIO_InitStruct);
}

static void MX_I2C1_Init(void) {
    // ... (MX_I2C1_Init function remains the same)
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void MX_TIM2_Init(void) {
    // ... (MX_TIM2_Init function remains the same)
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 65535;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim2);
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);
}

void delay_us(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}

void DHT11_Start(void) {
    // ... (DHT11_Start function remains the same)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30);
    
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

uint8_t DHT11_Check_Response(void) {
    // ... (DHT11_Check_Response function remains the same)
    uint8_t response = 0;
    uint32_t timeout = 100;
    
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) && __HAL_TIM_GET_COUNTER(&htim2) < timeout);
    
    if (__HAL_TIM_GET_COUNTER(&htim2) >= timeout) return 0;
    
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    timeout = 100;
    while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) && __HAL_TIM_GET_COUNTER(&htim2) < timeout);
    
    if (__HAL_TIM_GET_COUNTER(&htim2) >= timeout) return 0;
    
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) && __HAL_TIM_GET_COUNTER(&htim2) < timeout);
    
    if (__HAL_TIM_GET_COUNTER(&htim2) >= timeout) return 0;
    
    return 1;
}

uint8_t DHT11_Read(void) {
    // ... (DHT11_Read function remains the same)
    uint8_t i, j = 0;
    uint32_t timeout = 100;
    
    for (i = 0; i < 8; i++) {
        __HAL_TIM_SET_COUNTER(&htim2, 0);
        while (!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) && __HAL_TIM_GET_COUNTER(&htim2) < timeout);
        
        if (__HAL_TIM_GET_COUNTER(&htim2) >= timeout) break;
        
        delay_us(40);
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) {
            j |= (1 << (7 - i));
        }
        
        __HAL_TIM_SET_COUNTER(&htim2, 0);
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) && __HAL_TIM_GET_COUNTER(&htim2) < timeout);
    }
    return j;
}

uint8_t read_DHT11(void) {
    // ... (read_DHT11 function remains the same)
    uint8_t i, checksum;
    
    DHT11_Start();
    if (DHT11_Check_Response()) {
        dht11_data[0] = DHT11_Read();
        dht11_data[1] = DHT11_Read();
        dht11_data[2] = DHT11_Read();
        dht11_data[3] = DHT11_Read();
        dht11_data[4] = DHT11_Read();
        
        checksum = dht11_data[0] + dht11_data[1] + dht11_data[2] + dht11_data[3];
        if (checksum == dht11_data[4]) {
            current_temperature = dht11_data[2];
            return 1;
        }
    }
    return 0;
}

uint32_t read_HCSR04(void) {
    // ... (read_HCSR04 function remains the same)
    uint32_t start_time = 0, end_time = 0;
    uint32_t timeout = 30000;
    
    HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_RESET);

    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET) {
        if (__HAL_TIM_GET_COUNTER(&htim2) > timeout) {
            return 0;
        }
    }
    
    start_time = __HAL_TIM_GET_COUNTER(&htim2);
    
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET) {
        if (__HAL_TIM_GET_COUNTER(&htim2) > (start_time + timeout)) {
            return 999;
        }
    }
    end_time = __HAL_TIM_GET_COUNTER(&htim2);

    uint32_t pulse_duration = end_time - start_time;
    uint32_t distance = (pulse_duration * 17) / 1000;
    
    if (distance > 20) {
        return 999;
    }
    
    return distance;
}

char read_keypad(void) {
    static uint32_t last_press_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // Debounce
    if (current_time - last_press_time < 200) {
        return 0;
    }
    
    // Reset all rows to High (inactive) before scanning
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_SET);

    // Scan Row 2 for key '5' (DHT Mode) on Column 2
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW2_PIN, GPIO_PIN_RESET); // Activate Row 2
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(KEYPAD_PORT, COL2_PIN) == GPIO_PIN_RESET) {
        HAL_GPIO_WritePin(KEYPAD_PORT, ROW2_PIN, GPIO_PIN_SET);
        last_press_time = current_time;
        return '5';
    }
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW2_PIN, GPIO_PIN_SET);

    // Scan Row 3 for key '8' (Sonar Mode) on Column 2
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_RESET); // Activate Row 3
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(KEYPAD_PORT, COL2_PIN) == GPIO_PIN_RESET) {
        HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_SET);
        last_press_time = current_time;
        return '8';
    }
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_SET);

    // Scan Row 3 for key '9' (Return) on Column 3 <--- CHANGED
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_RESET); // Activate Row 3
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(KEYPAD_PORT, COL3_PIN) == GPIO_PIN_RESET) { // <--- CHECKING COL3_PIN (PA9)
        HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_SET);
        last_press_time = current_time;
        return '9';
    }
    HAL_GPIO_WritePin(KEYPAD_PORT, ROW3_PIN, GPIO_PIN_SET); // Reset

    // Note: Key '2' (Row 1, Col 2) is no longer being actively scanned to focus on the menu keys.
    // Row 4 is also no longer being actively scanned.

    return 0;
}

void blink_led(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }
}

void fast_blink_led(void) {
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}
