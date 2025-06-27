#include "stm32f4xx.h"
#include <cstring>
#include <stdbool.h>

using std::memcmp;
using std::memcpy;

#define UART_BUF_SIZE 64
const uint8_t ID[4] = {0x41, 0x42, 0x43, 0x44}; // "ABCD"

void SYSCLK(void) {
    // Включаем HSE
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Выключаем PLL
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    // Настраиваем PLL
    RCC->PLLCFGR = 0;
    RCC->PLLCFGR |= (25U << RCC_PLLCFGR_PLLM_Pos);   // PLLM = 25
    RCC->PLLCFGR |= (192U << RCC_PLLCFGR_PLLN_Pos);  // PLLN = 192
    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLP;               // PLLP = 2
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;          // Источник - HSE
    RCC->PLLCFGR |= (8U << RCC_PLLCFGR_PLLQ_Pos);    // PLLQ = 8 USB не нужен, но пусть будет

    // Включаем PLL
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // Настраиваем Flash
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_3WS;

    // Делители шин
    RCC->CFGR &= ~RCC_CFGR_HPRE;               // AHB = /1
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;          // APB1 = /2 (USART2 здесь)
    RCC->CFGR &= ~RCC_CFGR_PPRE2;              // APB2 = /1

    // Переключаемся на PLL как системную тактовую
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    SystemCoreClockUpdate();
}

void UART2_CONF(void) {
    // Тактирование USART2 и GPIOA
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // Настройка PA2 (TX) и PA3 (RX)
    GPIOA->MODER &= ~(GPIO_MODER_MODE2 | GPIO_MODER_MODE3);
    GPIOA->MODER |= (GPIO_MODER_MODE2_1 | GPIO_MODER_MODE3_1); // альтернативная функция
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);     // push-pull
    GPIOA->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR2 | GPIO_OSPEEDER_OSPEEDR3); // высокая скорость
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3);  // без pull-up/down

    // Альтернативная функция AF7 (USART2)
    GPIOA->AFR[0] &= ~((0xF << (2 * 4)) | (0xF << (3 * 4))); // очистка
    GPIOA->AFR[0] |= (7 << (2 * 4)) | (7 << (3 * 4));        // установка AF7

    // Отключаем OVER8
    USART2->CR1 &= ~USART_CR1_OVER8;

    // Частота APB1 = 48 MHz USARTDIV = 48e6 / (16 * 115200) ≈ 26.0417
    // BRR = (26 << 4) | 1
    USART2->BRR = (26 << 4) | 1;

    // Разрешаем приём, передачу, прерывание по приёму, включаем USART
    USART2->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;
		// Разрешаем прерывание в NVIC
    NVIC_EnableIRQ(USART2_IRQn);
    
}


int main(void) {
  SYSCLK();
  UART2_CONF();

    while (1) {

    }
}

extern "C" void USART2_IRQHandler(void) {
    // Проверка флага приёма
    if (USART2->SR & USART_SR_RXNE) {
        uint8_t data = USART2->DR; // Чтение данных (сбрасывает флаг)
		}
}