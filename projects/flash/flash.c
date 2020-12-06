/*
 * flash.c
 *
 * description:
 *   erase sector 3 and write 4-bytes of data to flash
 *   careful with flash - have a button guard so that
 *   it does not execute every time the system boots up
 *
 *   It assumes the board runs with default USB power
 */

#include "stm32f4xx.h"
#include "system_stm32f4xx.h"

/*************************************************
* function declarations
*************************************************/
int main(void);

// flash unlock sequence
#define KEY1 0x45670123
#define KEY2 0xCDEF89AB

// variable that will be hold in the flash
#define VARADDR 0x0800C000 // address that will hold the variable
#define VAR     0x12345670 // variable value

void unlock_flash() {
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = KEY1;
        FLASH->KEYR = KEY2;
    }
}

void lock_flash() {
    FLASH->CR |= FLASH_CR_LOCK; // bit 31
}

void erase_flash_sector3() {
    const uint32_t sec = 3;
    __disable_irq();
    while(FLASH->SR & FLASH_SR_BSY); // check if busy
    FLASH->CR |= FLASH_CR_SER;
    FLASH->CR |= (sec << 3); // SNB bit 3:6
    FLASH->CR |= FLASH_CR_STRT; // start
    while(FLASH->SR & FLASH_SR_BSY); // check if busy
    FLASH->CR &= ~FLASH_CR_SER; // clear SER bit
    FLASH->CR &= ~(0xFU << 3); // clear SNB bit 3:6
    __enable_irq();
}

void write_flash(uint32_t addr, uint32_t data){
    while(FLASH->SR & FLASH_SR_BSY); // check if busy
    FLASH->CR |= FLASH_CR_PG;
    FLASH->CR &= ~(0x3U << 8); // clear PSIZE bit 8:9
    FLASH->CR |= (0x2 << 8);   // program PSIZE
    *(volatile uint32_t*)addr = data;
    while(FLASH->SR & FLASH_SR_BSY); // check if busy
    FLASH->CR &= (~FLASH_CR_PG); // disable PG bit
}

/*************************************************
* main code starts from here
*************************************************/
int main(void)
{
    /* set system clock to 168 Mhz */
    set_sysclk_to_168();

    /* set up button */
    // enable GPIOA clock (AHB1ENR: bit 0)
    RCC->AHB1ENR |= (1 << 0);
    GPIOA->MODER &= 0xFFFFFFFC;   // Reset bits 0-1 to clear old values
    GPIOA->MODER |= 0x00000000;   // Make button an input

    // wait a little bit to give time for button press
    volatile int i = 0;
    for(i=0; i<100000;i++);

    // only update if button is pressed;
    if(GPIOA->IDR & 0x01) {
        // read / write op
        unlock_flash();

        erase_flash_sector3();
        write_flash(VARADDR, VAR);
        // do read write
        lock_flash();
    }


    while(1)
    {
    }

    return 0;
}
