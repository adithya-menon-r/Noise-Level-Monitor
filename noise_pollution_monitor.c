#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define BUTTON_PIN 0
#define ADC_CHANNEL 1

// Display buffer
static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

volatile uint32_t msTicks = 0;

// Display modes
typedef enum {
    DISPLAY_CURRENT = 0,
    DISPLAY_MAX,
    DISPLAY_MIN,
    DISPLAY_STATS
} DisplayMode_t;

DisplayMode_t current_mode = DISPLAY_CURRENT;

float max_db = 0.0f;
float min_db = 999.0f;
uint16_t max_adc = 0;
uint16_t min_adc = 4095;

// Time tracking for different sound levels
uint32_t time_low = 0; // 0-60 dB
uint32_t time_medium = 0; // 60-90 dB
uint32_t time_high = 0; // 90+ dB
uint32_t last_category_update = 0;

const uint8_t font5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x7C, 0x12, 0x11, 0x12, 0x7C},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x2F, 0x00, 0x00},
    {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x7C, 0x08, 0x04, 0x04, 0x78},
};

void SystemClock_Config(void);
void I2C1_Init(void);
void ADC1_Init(void);
void Button_Init(void);
void Delay_ms(uint32_t ms);
void EXTI0_IRQHandler(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Write(uint8_t data);
void OLED_Command(uint8_t cmd);
void OLED_Data(uint8_t data);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Display(void);
void OLED_WriteChar(char c, uint8_t x, uint8_t y);
void OLED_WriteString(char* str, uint8_t x, uint8_t y);
uint16_t ADC_Read(void);
float ADC_to_dB(uint16_t adc_value);
void UpdateDisplay(float db_value, uint16_t adc_value);

volatile uint8_t button_pressed = 0;

int main(void) {
    uint16_t adc_value;
    float db_value;
    uint16_t threshold = 800; 
    uint32_t last_display_update = 0;
    
    SystemClock_Config();
    SysTick_Config(84000);
    
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3<<26);
    GPIOC->MODER |= (1<<26);
    GPIOC->ODR |= (1<<13);
    
    Delay_ms(100);
    I2C1_Init();
    Delay_ms(100);
    ADC1_Init();
    Delay_ms(100);
    Button_Init();
    
    OLED_Init();
    OLED_Clear();
    OLED_WriteString("ES Case Study", 10, 0);
    OLED_WriteString("Initialising...", 10, 24);
    OLED_Display();
    Delay_ms(1000);
    
    last_category_update = msTicks;
    
    while(1) {
        adc_value = ADC_Read();
        
        if(adc_value > threshold) {
            GPIOC->ODR &= ~(1<<13);
        } else {
            GPIOC->ODR |= (1<<13);
        }
        
        // Convert to ADC val to dB val
        db_value = ADC_to_dB(adc_value);
        
        // Tracking time in each sound level category
        uint32_t current_time = msTicks;
        if(last_category_update > 0) {
            uint32_t elapsed = current_time - last_category_update;
            
            if(db_value < 60.0f) {
                time_low += elapsed;
            } else if(db_value < 90.0f) {
                time_medium += elapsed;
            } else {
                time_high += elapsed;
            }
        }
        last_category_update = current_time;
        
        if(db_value > max_db) {
            max_db = db_value;
            max_adc = adc_value;
        }
        if(db_value < min_db && adc_value > 50) {
            min_db = db_value;
            min_adc = adc_value;
        }
        
        if(button_pressed) {
            button_pressed = 0;
            
            current_mode = (DisplayMode_t)((current_mode + 1) % 4);
            
            OLED_Clear();
            UpdateDisplay(db_value, adc_value);
        }
        
        // Updating the OLED display every 50ms
        if((msTicks - last_display_update) >= 50) {
            last_display_update = msTicks;
            UpdateDisplay(db_value, adc_value);
        }
        
        Delay_ms(5);
    }
}

void Button_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    
    GPIOA->MODER &= ~(3 << (BUTTON_PIN * 2));
    
    GPIOA->PUPDR &= ~(3 << (BUTTON_PIN * 2));
    GPIOA->PUPDR |= (1 << (BUTTON_PIN * 2));
    
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PA;
    
    EXTI->IMR |= EXTI_IMR_MR0;
    EXTI->FTSR |= EXTI_FTSR_TR0;
    
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI0_IRQn, 2);
}

void EXTI0_IRQHandler(void) {
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = msTicks;
    
    // check if EXTI0 triggered the interrupt
    if(EXTI->PR & EXTI_PR_PR0) {
        // Debouncing
        if((current_time - last_interrupt_time) > 200) {
            button_pressed = 1;
            last_interrupt_time = current_time;
            
            GPIOC->ODR &= ~(1<<13);
            for(volatile int i = 0; i < 100000; i++);
            GPIOC->ODR |= (1<<13);
        }
        
        EXTI->PR = EXTI_PR_PR0;
    }
}

void UpdateDisplay(float db_value, uint16_t adc_value) {
    char buffer[32];
    int db_int, db_frac;
    
    // Clear display area
    for(int page = 0; page < 8; page++) {
        for(int col = 0; col < 128; col++) {
            oled_buffer[page * 128 + col] = 0x00;
        }
    }
    
    // Display based on mode
    switch(current_mode) {
        case DISPLAY_CURRENT:
            OLED_WriteString("Sound Level", 10, 0);
            OLED_WriteString("Current:", 5, 20);
            
            db_int = (int)db_value;
            db_frac = (int)((db_value - db_int) * 10);
            sprintf(buffer, "%d.%d dB", db_int, db_frac);
            OLED_WriteString(buffer, 20, 36);
            
            sprintf(buffer, "ADC:%d", adc_value);
            OLED_WriteString(buffer, 15, 52);
            break;
            
        case DISPLAY_MAX:
            OLED_WriteString("Sound Level", 10, 0);
            OLED_WriteString("Maximum:", 5, 20);
            
            db_int = (int)max_db;
            db_frac = (int)((max_db - db_int) * 10);
            sprintf(buffer, "%d.%d dB", db_int, db_frac);
            OLED_WriteString(buffer, 20, 36);
            
            sprintf(buffer, "ADC:%d", max_adc);
            OLED_WriteString(buffer, 15, 52);
            break;
            
        case DISPLAY_MIN:
            OLED_WriteString("Sound Level", 10, 0);
            OLED_WriteString("Minimum:", 5, 20);
            
            db_int = (int)min_db;
            db_frac = (int)((min_db - db_int) * 10);
            sprintf(buffer, "%d.%d dB", db_int, db_frac);
            OLED_WriteString(buffer, 20, 36);
            
            sprintf(buffer, "ADC:%d", min_adc);
            OLED_WriteString(buffer, 15, 52);
            break;
            
        case DISPLAY_STATS:
            OLED_WriteString("Time Stats", 15, 0);
            
            uint32_t low_sec = time_low / 1000;
            uint32_t low_h = low_sec / 3600;
            uint32_t low_m = (low_sec % 3600) / 60;
            uint32_t low_s = low_sec % 60;
            sprintf(buffer, "Low:%02lu:%02lu:%02lu", low_h, low_m, low_s);
            OLED_WriteString(buffer, 5, 20);
            
            uint32_t med_sec = time_medium / 1000;
            uint32_t med_h = med_sec / 3600;
            uint32_t med_m = (med_sec % 3600) / 60;
            uint32_t med_s = med_sec % 60;
            sprintf(buffer, "Med:%02lu:%02lu:%02lu", med_h, med_m, med_s);
            OLED_WriteString(buffer, 5, 36);
            
            uint32_t high_sec = time_high / 1000;
            uint32_t high_h = high_sec / 3600;
            uint32_t high_m = (high_sec % 3600) / 60;
            uint32_t high_s = high_sec % 60;
            sprintf(buffer, "High:%02lu:%02lu:%02lu", high_h, high_m, high_s);
            OLED_WriteString(buffer, 5, 52);
            break;
    }
    
    OLED_Display();
}

// Convert ADC value to dB val
float ADC_to_dB(uint16_t adc_value) {
    // Calibration values
    #define BASELINE_ADC 792.0f
    #define REFERENCE_DB 40.0f
    #define SENSITIVITY 20.0f

    // Calculate absolute diff from baseline
    float adc_diff = fabs((float)adc_value - BASELINE_ADC);
    
    adc_diff += 1.0f;
    
    float db = REFERENCE_DB + (SENSITIVITY * log10f(adc_diff));
    
    if(db < 30.0f) db = 30.0f;
    if(db > 110.0f) db = 110.0f;
    
    return db;
}

void SystemClock_Config(void) {
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));
    FLASH->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    RCC->PLLCFGR = (84 << 6) | (16 << 0) | (0 << 16) | (2 << 24);
    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void I2C1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    
    GPIOB->MODER &= ~((3<<12) | (3<<14));
    GPIOB->MODER |= (2<<12) | (2<<14);
    GPIOB->AFR[0] &= ~((0xF<<24) | (0xF<<28));
    GPIOB->AFR[0] |= (4<<24) | (4<<28);
    GPIOB->OTYPER |= (1<<6) | (1<<7);
    GPIOB->OSPEEDR |= (3<<12) | (3<<14);
    GPIOB->PUPDR &= ~((3<<12) | (3<<14));
    GPIOB->PUPDR |= (1<<12) | (1<<14);
    
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;
    I2C1->CR2 = 42;
    I2C1->CCR = 210;
    I2C1->TRISE = 43;
    I2C1->CR1 |= I2C_CR1_PE;
    Delay_ms(10);
}

void ADC1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    
    GPIOA->MODER |= (3 << (ADC_CHANNEL * 2)); // Analog mode for PA1
    
    ADC1->CR2 = 0;
    ADC1->CR1 = 0;
    ADC1->CR1 &= ~ADC_CR1_RES;
    ADC1->CR2 &= ~ADC_CR2_CONT;
    ADC1->CR2 &= ~ADC_CR2_ALIGN;
    
    ADC1->SMPR2 |= (0x04 << (ADC_CHANNEL * 3)); // seeting sampling time as 84 cycles for Channel 1
    
    ADC1->SQR1 = 0;
    ADC1->SQR3 = ADC_CHANNEL;
    ADC1->CR2 |= ADC_CR2_ADON;
    Delay_ms(1);
}

uint16_t ADC_Read(void) {
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while(!(ADC1->SR & ADC_SR_EOC));
    return ADC1->DR;
}

void I2C_Start(void) {
    I2C1->CR1 |= I2C_CR1_START;
    while(!(I2C1->SR1 & I2C_SR1_SB));
}

void I2C_Stop(void) {
    I2C1->CR1 |= I2C_CR1_STOP;
}

void I2C_Write(uint8_t data) {
    while(!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = data;
    while(!(I2C1->SR1 & I2C_SR1_TXE));
}

void OLED_Command(uint8_t cmd) {
    I2C_Start();
    I2C1->DR = (OLED_I2C_ADDR << 1);
    while(!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;
    I2C_Write(0x00);
    I2C_Write(cmd);
    I2C_Stop();
    Delay_ms(1);
}

void OLED_Data(uint8_t data) {
    I2C_Start();
    I2C1->DR = (OLED_I2C_ADDR << 1);
    while(!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;
    I2C_Write(0x40);
    I2C_Write(data);
    I2C_Stop();
}

void OLED_Init(void) {
    Delay_ms(100);
    OLED_Command(0xAE);
    OLED_Command(0xD5);
    OLED_Command(0x80);
    OLED_Command(0xA8);
    OLED_Command(0x3F);
    OLED_Command(0xD3);
    OLED_Command(0x00);
    OLED_Command(0x40);
    OLED_Command(0x8D);
    OLED_Command(0x14);
    OLED_Command(0x20);
    OLED_Command(0x00);
    OLED_Command(0xA1);
    OLED_Command(0xC8);
    OLED_Command(0xDA);
    OLED_Command(0x12);
    OLED_Command(0x81);
    OLED_Command(0xCF);
    OLED_Command(0xD9);
    OLED_Command(0xF1);
    OLED_Command(0xDB);
    OLED_Command(0x40);
    OLED_Command(0xA4);
    OLED_Command(0xA6);
    OLED_Command(0xAF);
    OLED_Clear();
}

void OLED_Clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
    OLED_Display();
}

void OLED_Display(void) {
    for(uint8_t page = 0; page < 8; page++) {
        OLED_Command(0xB0 + page);
        OLED_Command(0x00);
        OLED_Command(0x10);
        
        I2C_Start();
        I2C1->DR = (OLED_I2C_ADDR << 1);
        while(!(I2C1->SR1 & I2C_SR1_ADDR));
        (void)I2C1->SR2;
        I2C_Write(0x40);
        
        for(uint8_t col = 0; col < 128; col++) {
            I2C_Write(oled_buffer[page * 128 + col]);
        }
        I2C_Stop();
    }
}

void OLED_WriteChar(char c, uint8_t x, uint8_t y) {
    uint8_t idx = 255;
    
    if(c >= '0' && c <= '9') {
        idx = c - '0';
    } else if(c >= 'A' && c <= 'Z') {
        idx = c - 'A' + 10;
    } else if(c >= 'a' && c <= 'z') {
        idx = c - 'a' + 10;
    } else if(c == ' ') {
        idx = 36;
    } else if(c == ':') {
        idx = 38;
    } else if(c == '-') {
        idx = 39;
    } else if(c == '.') {
        idx = 40;
    } else if(c == 'd') {
        idx = 40;
    } else {
        return;
    }
    
    for(uint8_t i = 0; i < 5; i++) {
        if(x + i < 128) {
            oled_buffer[(y/8)*128 + x + i] = font5x7[idx][i];
        }
    }
}

void OLED_WriteString(char* str, uint8_t x, uint8_t y) {
    while(*str) {
        OLED_WriteChar(*str++, x, y);
        x += 6;
        if(x >= 122) break;
    }
}

void SysTick_Handler(void) {
    msTicks++;
}

void Delay_ms(uint32_t ms) {
    uint32_t start = msTicks;
    while((msTicks - start) < ms);
}