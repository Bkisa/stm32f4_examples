/*
 * i2s-beep.c
 *
 * description:
 *   talk to CS43L22 Audio DAC over I2C
 *   connected to I2C1 PB6, PB9
 *   setups CS43L22 to play 6 beep sounds from headphone jack
 *   Audio out is connected to I2S3
 */

#include "stm32f4xx.h"
#include "system_stm32f4xx.h"
#include "cs43l22.h"

/*************************************************
* function declarations
*************************************************/
int main(void);
void init_i2s_pll();
void init_i2s3();
void init_cs43l22(uint8_t);
void start_cs43l22();

/*************************************************
* I2C related general functions
*************************************************/
volatile uint8_t DeviceAddr = CS43L22_ADDRESS;

static inline void __i2c_start() {
    I2C1->CR1 |= I2C_CR1_START;
    while(!(I2C1->SR1 & I2C_SR1_SB));
}

static inline void __i2c_stop() {
    I2C1->CR1 |= I2C_CR1_STOP;
    while(!(I2C1->SR2 & I2C_SR2_BUSY));
}

void i2c_write(uint8_t regaddr, uint8_t data) {
    // send start condition
    __i2c_start();

    // send chipaddr in write mode
    // wait until address is sent
    I2C1->DR = DeviceAddr;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    // dummy read to clear flags
    (void)I2C1->SR2; // clear addr condition

    // send MAP byte with auto increment off
    // wait until byte transfer complete (BTF)
    I2C1->DR = regaddr;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    // send data
    // wait until byte transfer complete
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    // send stop condition
    __i2c_stop();
}

uint8_t i2c_read(uint8_t regaddr) {
    uint8_t reg;

    // send start condition
    __i2c_start();

    // send chipaddr in write mode
    // wait until address is sent
    I2C1->DR = DeviceAddr;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    // dummy read to clear flags
    (void)I2C1->SR2; // clear addr condition

    // send MAP byte with auto increment off
    // wait until byte transfer complete (BTF)
    I2C1->DR = regaddr;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    // restart transmission by sending stop & start
    __i2c_stop();
    __i2c_start();

    // send chipaddr in read mode. LSB is 1
    // wait until address is sent
    I2C1->DR = DeviceAddr | 0x01; // read
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    // dummy read to clear flags
    (void)I2C1->SR2; // clear addr condition

    // wait until receive buffer is not empty
    while (!(I2C1->SR1 & I2C_SR1_RXNE));
    // read content
    reg = (uint8_t)I2C1->DR;

    // send stop condition
    __i2c_stop();

    return reg;
}

/*************************************************
* I2S related functions
*************************************************/

// enable I2S pll
void init_i2s_pll() {
    // enable PLL I2S for 48khz Fs
    // for VCO = 1Mhz (8Mhz / M = 8Mhz / 8)
    // I2SxCLK = VCO x N / R
    // for N = 258, R = 3 => I2SxCLK = 86Mhz
    RCC->PLLI2SCFGR |= (258 << 6); // N value = 258
    RCC->PLLI2SCFGR |= (3 << 28); // R value = 3
    RCC->CR |= (1 << 26); // enable PLLI2SON
    while(!(RCC->CR & (1 << 27))); // wait until PLLI2SRDY
}

/* Setup I2S for CS43L22 Audio DAC
 * Pins are connected to
 * PC7 - MCLK, PC10 - SCK, PC12 - SD, PA4 - WS
 */
void init_i2s3() {
    // Setup pins PC7 - MCLK, PC10 - SCK, PC12 - SD, PA4 - WS
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN);
    RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    // PC7 alternate function mode MCLK
    GPIOC->MODER   &= ~(3U << 7*2);
    GPIOC->MODER   |= (2 << 7*2);
    GPIOC->OSPEEDR |= (3 << 7*2);
    GPIOC->AFR[0]  |= (6 << 7*4);
    // PC10 alternate function mode SCL
    GPIOC->MODER   &= ~(3U << 10*2);
    GPIOC->MODER   |= (2 << 10*2);
    GPIOC->OSPEEDR |= (3 << 10*2);
    GPIOC->AFR[1]  |= (6 << (10-8)*4);
    // PC12 alternate function mode SD
    GPIOC->MODER   &= ~(3U << 12*2);
    GPIOC->MODER   |= (2 << 12*2);
    GPIOC->OSPEEDR |= (3 << 12*2);
    GPIOC->AFR[1]  |= (6 << (12-8)*4);
    // PA4 alternate function mode WS
    GPIOA->MODER   &= ~(3U << 4*2);
    GPIOA->MODER   |= (2 << 4*2);
    GPIOA->OSPEEDR |= (3 << 4*2);
    GPIOA->AFR[0]  |= (6 << 4*4);

    // Configure I2S
    SPI3->I2SCFGR = 0; // reset registers
    SPI3->I2SPR   = 0; // reset registers
    SPI3->I2SCFGR |= (1 << 11); // I2S mode is selected
    // I2S config mode
    // 10 - Master Transmit
    // 11 - Master Receive
    // Since we will just use built-in beep, we can set it up as receive
    // mode to always activate clock.
    SPI3->I2SCFGR |= (3 << 8);

    // have no effect
    SPI3->I2SCFGR |= (0 << 7);  // PCM frame sync, 0 - short frame
    SPI3->I2SCFGR |= (0 << 4);  // I2S standard select, 00 Philips standard, 11 PCM standard
    SPI3->I2SCFGR |= (0 << 3);  // Steady state clock polarity, 0 - low, 1 - high
    SPI3->I2SCFGR |= (0 << 0);  // Channel length, 0 - 16bit, 1 - 32bit

    SPI3->I2SPR |= (1 << 9); // Master clock output enable
    // 48 Khz
    SPI3->I2SPR |= (1 << 8); // Odd factor for the prescaler (I2SODD)
    SPI3->I2SPR |= (3 << 0); // Linear prescaler (I2SDIV)

    SPI3->I2SCFGR |= (1 << 10); // I2S enabled
}

/*************************************************
* CS43L22 related functions
*************************************************/

void init_cs43l22(uint8_t an_ch) {
    // setup reset pin for CS43L22 - GPIOD 4
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    GPIOD->MODER &= ~(3U << 4*2);
    GPIOD->MODER |=  (1 << 4*2);
    // activate CS43L22
    GPIOD->ODR   |=  (1 << 4);

    uint8_t data;
    // power off
    i2c_write(CS43L22_REG_POWER_CTL1, CS43L22_PWR_CTRL1_POWER_DOWN);

    // headphones on, speakers off
    data = (2 << 6) | (2 << 4) | (3 << 2) | (3 << 0);
    i2c_write(CS43L22_REG_POWER_CTL2, data);

    // auto detect clock
    data = (1 << 7);
    i2c_write(CS43L22_REG_CLOCKING_CTL, data);

    // slave mode, DSP mode disabled, I2S data format, 16-bit data
    data = (1 << 2) | (3 << 0);
    i2c_write(CS43L22_REG_INTERFACE_CTL1, data);

    // select ANx as passthrough source based on the parameter passed
    if ((an_ch > 0) && (an_ch < 5)) {
        data = (uint8_t)(1 << (an_ch-1));
    }
    else {
        data = 0;
    }
    i2c_write(CS43L22_REG_PASSTHR_A_SELECT, data);
    i2c_write(CS43L22_REG_PASSTHR_B_SELECT, data);

    // ganged control of both channels
    data = (1 << 7);
    i2c_write(CS43L22_REG_PASSTHR_GANG_CTL, data);

    // playback control 1
    // hp gain 0.6, single control, master playback
    data = (3 << 5) | (1 << 4);
    i2c_write(CS43L22_REG_PLAYBACK_CTL1, data);

    // misc controls,
    // passthrough analog enable/disable
    // passthrough mute/unmute
    if ((an_ch > 0) && (an_ch < 5)) {
        data = (1 << 7) | (1 << 6);
    }
    else {
        data = (1 << 5) | (1 << 4); // mute
    }
    i2c_write(CS43L22_REG_MISC_CTL, data);

    // passthrough volume
    data = 0; // 0 dB
    i2c_write(CS43L22_REG_PASSTHR_A_VOL, data);
    i2c_write(CS43L22_REG_PASSTHR_B_VOL, data);

    // pcm volume
    data = 0; // 0 dB
    i2c_write(CS43L22_REG_PCMA_VOL, data);
    i2c_write(CS43L22_REG_PCMB_VOL, data);

    start_cs43l22();
}

void start_cs43l22() {
    // initialization sequence from the data sheet pg 32
    // write 0x99 to register 0x00
    i2c_write(0x00, 0x99);
    // write 0x80 to register 0x47
    i2c_write(0x47, 0x80);
    // write 1 to bit 7 in register 0x32
    uint8_t data = i2c_read(0x32);
    data |= (1 << 7);
    i2c_write(0x32, data);
    // write 0 to bit 7 in register 0x32
    data &= (uint8_t)(~(1U << 7));
    i2c_write(0x32, data);
    // write 0x00 to register 0x00
    i2c_write(0, 0x00);

    // power on
    i2c_write(CS43L22_REG_POWER_CTL1, CS43L22_PWR_CTRL1_POWER_UP);
    // wait little bit
    for (volatile int i=0; i<500000; i++);
}

void I2C1_ER_IRQHandler(){
    // error handler
    GPIOD->ODR |= (1 << 14); // red LED
}

/*************************************************
* main code starts from here
*************************************************/
int main(void)
{
    /* set system clock to 168 Mhz */
    set_sysclk_to_168();

    //*******************************
    // setup LEDs - GPIOD 12,13,14,15
    //*******************************
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    GPIOD->MODER &= ~(0xFFU << 24);
    GPIOD->MODER |= (0x55 << 24);
    GPIOD->ODR    = 0x0000;

    //*******************************
    // setup I2C - GPIOB 6, 9
    //*******************************
    // enable I2C clock
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    // setup I2C pins
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER &= ~(3U << 6*2); // PB6
    GPIOB->MODER |=  (2 << 6*2); // AF
    GPIOB->OTYPER |= (1 << 6);   // open-drain
    GPIOB->MODER &= ~(3U << 9*2); // PB9
    GPIOB->MODER |=  (2 << 9*2); // AF
    GPIOB->OTYPER |= (1 << 9);   // open-drain

    // choose AF4 for I2C1 in Alternate Function registers
    GPIOB->AFR[0] |= (4 << 6*4);     // for pin 6
    GPIOB->AFR[1] |= (4 << (9-8)*4); // for pin 9

    // reset and clear reg
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;

    I2C1->CR2 |= (I2C_CR2_ITERREN); // enable error interrupt

    // fPCLK1 must be at least 2 Mhz for SM mode
    //        must be at least 4 Mhz for FM mode
    //        must be multiple of 10Mhz to reach 400 kHz
    // DAC works at 100 khz (SM mode)
    // For SM Mode:
    //    Thigh = CCR * TPCLK1
    //    Tlow  = CCR * TPCLK1
    // So to generate 100 kHz SCL frequency
    // we need 1/100kz = 10us clock speed
    // Thigh and Tlow needs to be 5us each
    // Let's pick fPCLK1 = 10Mhz, TPCLK1 = 1/10Mhz = 100ns
    // Thigh = CCR * TPCLK1 => 5us = CCR * 100ns
    // CCR = 50
    I2C1->CR2 |= (10 << 0); // 10Mhz periph clock
    I2C1->CCR |= (50 << 0);
    // Maximum rise time.
    // Calculation is (maximum_rise_time / fPCLK1) + 1
    // In SM mode maximum allowed SCL rise time is 1000ns
    // For TPCLK1 = 100ns => (1000ns / 100ns) + 1= 10 + 1 = 11
    I2C1->TRISE |= (11 << 0); // program TRISE to 11 for 100khz
    // set own address to 00 - not really used in master mode
    I2C1->OAR1 |= (0x00 << 1);
    I2C1->OAR1 |= (1 << 14); // bit 14 should be kept at 1 according to the datasheet

    // enable error interrupt from NVIC
    NVIC_SetPriority(I2C1_ER_IRQn, 1);
    NVIC_EnableIRQ(I2C1_ER_IRQn);

    I2C1->CR1 |= I2C_CR1_PE; // enable i2c

    // audio PLL
    init_i2s_pll();
    // audio out
    init_i2s3();
    // initialize audio dac
    init_cs43l22(0);

    // read Chip ID - first 5 bits of CHIP_ID_ADDR
    uint8_t ret = i2c_read(CS43L22_REG_ID);

    if ((ret >> 3) != CS43L22_CHIP_ID) {
        GPIOD->ODR |= (1 << 13); // orange led on error
    }

    // beep volume
    uint8_t vol = (0x1C - 12); // -6 - 12*2 dB
    i2c_write(CS43L22_REG_BEEP_VOL_OFF_TIME, vol);

    // EGBEBG
    uint8_t nem[6] = {0x31, 0x51, 0x71, 0xA1, 0x71, 0x51};
    i2c_write(CS43L22_REG_BEEP_TONE_CFG, 0xC0);

    for (int i=0; i<6; i++) {
        i2c_write(CS43L22_REG_BEEP_FREQ_ON_TIME, nem[i]);
        for (volatile int j=0; j<5000000; j++);
        GPIOD->ODR ^= (1 << 12); // toggle green led
    }
    i2c_write(CS43L22_REG_BEEP_TONE_CFG, 0x00); // turn off

    GPIOD->ODR |= (1 << 15); // blue led
    while(1)
    {
    }
    return 0;
}
