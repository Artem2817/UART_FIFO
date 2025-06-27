#include "stm32f4xx.h"
#include <cstring>
#include <stdbool.h>

using std::memcmp;
using std::memcpy;

#define UART_BUF_SIZE 64
const uint8_t ID[4] = {0x41, 0x42, 0x43, 0x44}; // "ABCD"

///////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	uint8_t command; 
	uint8_t data[UART_BUF_SIZE];
} Messange_t;
//////////////////////////////////////////////////////////////////////////////////////

Messange_t MESS; // MESSANGE

//////////////////////////////////////////////////////////////////////////////////////
enum STATUS
{
	WH_ID,
	WH_LEN,
	WH_COM,
	WH_DATA,
	WH_CRC
};
//////////////////////////////////////////////////////////////////////////////////////

STATUS state;

//////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	uint8_t FIFO_data[UART_BUF_SIZE];
	volatile uint32_t head = 0;
	uint32_t tail = 0;
} FIFO_t;
//////////////////////////////////////////////////////////////////////////////////////

FIFO_t fifo_rx; // FIFO

uint32_t FIFO_Available()
{
	if ((fifo_rx.head - fifo_rx.tail) != 0 )
	{
		return (fifo_rx.head - fifo_rx.tail); // возвращаем скольок байт есть в fifo
	}
	else
	{
		return 0; // fifo пуст
	}
}

uint8_t FIFO_read()
{
	return fifo_rx.FIFO_data[(fifo_rx.tail++) & (UART_BUF_SIZE -1)];
}

void FIFO_write(uint8_t data)
{
	fifo_rx.FIFO_data[(fifo_rx.head++) & (UART_BUF_SIZE - 1)] = data;
}

uint8_t compute_CRC(uint8_t crc, uint8_t len, uint32_t comm, uint8_t* data)
{
	uint8_t sum_CRC = 0;
	sum_CRC = ID[0] + ID[1] + ID[2] + ID[3] + len + comm;
	for (int i = 0; i < len - 1; i++)
	{
		sum_CRC += data[i] + data[i + 1];
	}
	if (crc == sum_CRC)
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}

uint8_t Parse_FIFO_byte()
{
  static uint32_t success = 0;
  static uint32_t len = 0;
	static uint32_t command = 0;
	static uint8_t buff[UART_BUF_SIZE];
  uint8_t data;

  while (FIFO_Available() > 0)
  {
    switch (state)
    {
      case WH_ID:
        data = FIFO_read();
        if (data == ID[success])
        {
          success++;
          if (success >= 4)
          {
            state = WH_LEN;
            success = 0;
          }
        }
        else
        {
          success = 0;
        }
        break;

      case WH_LEN:
        data = FIFO_read();
        if (data != 0)
        {
          len = data;
          state = WH_COM;
        }
        else
        {
          state = WH_ID;
          return 0; // Некорректная длина
        }
        break;

      case WH_COM:
        command = FIFO_read();
        state = WH_DATA;
        break;

      case WH_DATA:
        if (FIFO_Available() >= len)
        {
          for (uint32_t i = 0; i < len - 1; i++)
          {
            buff[i] = FIFO_read();
          }
          state = WH_CRC;
        }
        else
        {
          return 3; // Недостаточно данных, выходим и ждём
        }
        break;

      case WH_CRC:
        if (FIFO_Available() > 0)
        {
          uint8_t crc = FIFO_read();
          state = WH_ID; // Переход в начальное состояние
          if (compute_CRC(crc, len, command, buff))
          {
						MESS.command = command;
						for (int i = 0; i < len; i++)
						{
							MESS.data[i] = buff[i];
						}
            return 1; // Всё ок
          }
          else
          {
            return 2; // Ошибка CRC
          }
        }
        else
        {
          return 3; // Недостаточно данных, ждём CRC
        }
        break;

      default:
        state = WH_ID;
        break;
    }
  }

  return 3; // Нет данных или не удалось завершить парсинг
}

void SYSCLK(void) 
{
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

void UART2_CONF(void) 
{
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


int main(void) 
{
  SYSCLK();
  UART2_CONF();
	uint8_t temp = 0;
    while (1) 
		{
			Parse_FIFO_byte();
			if (MESS.command == 1)
			{
				temp = MESS.data[0];
			}
    }
}

extern "C" void USART2_IRQHandler(void) 
{
    // Проверка флага приёма
    if (USART2->SR & USART_SR_RXNE) 
		{
			uint8_t data = USART2->DR; // Чтение данных (сбрасывает флаг)
			FIFO_write(data);
		}
}
