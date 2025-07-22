#include "ws2812.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_dma.h"

#define TIMING_ONE    60
#define TIMING_ZERO   30

#define MAX_LEDS      2100
#define BUFFER_SIZE   (MAX_LEDS * 24 + 43)

// TIM3_CH3 CCR3 寄存器地址
#define TIM3_CCR3_Address  ((uint32_t)&(TIM3->CCR3))

static uint16_t LED_BYTE_Buffer[BUFFER_SIZE];

/* 初始化 WS2812 GPIO/PWM/DMA */
void ws2812_init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    DMA_InitTypeDef         DMA_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // PB0 → TIM3_CH3 (AF2)
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 定时器配置（周期90约800kHz）
    TIM_TimeBaseStructure.TIM_Period        = 90 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler     = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // CH3 配置为 PWM1 模式
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_DMACmd(TIM3, TIM_DMA_Update, ENABLE);
    TIM_DMACmd(TIM3, TIM_DMA_CC3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    // DMA1_Stream7 / Channel5 配 TIM3_CH3
    DMA_DeInit(DMA1_Stream7);
    while (DMA_GetCmdStatus(DMA1_Stream7) != DISABLE);

    DMA_InitStructure.DMA_Channel            = DMA_Channel_5;
    DMA_InitStructure.DMA_PeripheralBaseAddr = TIM3_CCR3_Address;
    DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t)LED_BYTE_Buffer;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize         = BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_Init(DMA1_Stream7, &DMA_InitStructure);
}

/* 发送 LED 数据 */
void ws2812_send(uint8_t (*color)[3], uint16_t len)
{
    uint16_t memaddr = 0;
    uint16_t buffersize = len * 24 + 43;

    for (uint16_t j = 0; j < len; j++) {
        for (uint8_t i = 0; i < 8; i++)
            LED_BYTE_Buffer[memaddr++] = (color[j][1] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
        for (uint8_t i = 0; i < 8; i++)
            LED_BYTE_Buffer[memaddr++] = (color[j][0] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
        for (uint8_t i = 0; i < 8; i++)
            LED_BYTE_Buffer[memaddr++] = (color[j][2] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
    }

    LED_BYTE_Buffer[memaddr++] = TIMING_ZERO;
    while (memaddr < buffersize)
        LED_BYTE_Buffer[memaddr++] = 0;

    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    DMA_SetCurrDataCounter(DMA1_Stream7, buffersize);

    TIM_Cmd(TIM3, ENABLE);
    DMA_Cmd(DMA1_Stream7, ENABLE);
    while (DMA_GetFlagStatus(DMA1_Stream7, DMA_FLAG_TCIF7) == RESET);

    DMA_Cmd(DMA1_Stream7, DISABLE);
    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    TIM_Cmd(TIM3, DISABLE);
}

/* 设置某个像素的颜色数据（缓冲区） */
void ws2812_setPixelColor(uint8_t color[3], uint16_t num)
{
    uint16_t base = (num - 1) * 24;
    for (uint8_t i = 0; i < 8; i++)
        LED_BYTE_Buffer[base + i]      = (color[1] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
    for (uint8_t i = 0; i < 8; i++)
        LED_BYTE_Buffer[base + 8 + i]  = (color[0] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
    for (uint8_t i = 0; i < 8; i++)
        LED_BYTE_Buffer[base + 16 + i] = (color[2] & (0x80 >> i)) ? TIMING_ONE : TIMING_ZERO;
}

/* 显示前 sum 个像素 */
void ws2812_show(uint16_t sum)
{
    uint16_t buffersize = sum * 24 + 43;
    uint16_t memaddr = sum * 24;

    LED_BYTE_Buffer[memaddr++] = TIMING_ZERO;
    while (memaddr < buffersize)
        LED_BYTE_Buffer[memaddr++] = 0;

    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    DMA_SetCurrDataCounter(DMA1_Stream7, buffersize);

    TIM_Cmd(TIM3, ENABLE);
    DMA_Cmd(DMA1_Stream7, ENABLE);
    while (DMA_GetFlagStatus(DMA1_Stream7, DMA_FLAG_TCIF7) == RESET);

    DMA_Cmd(DMA1_Stream7, DISABLE);
    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    TIM_Cmd(TIM3, DISABLE);
}

/* 清除前 sum 个像素 */
void ws2812_clear(uint16_t sum)
{
    uint16_t buffersize = sum * 24 + 43;
    for (uint16_t i = 0; i < buffersize; i++)
        LED_BYTE_Buffer[i] = TIMING_ZERO;

    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    DMA_SetCurrDataCounter(DMA1_Stream7, buffersize);

    TIM_Cmd(TIM3, ENABLE);
    DMA_Cmd(DMA1_Stream7, ENABLE);
    while (DMA_GetFlagStatus(DMA1_Stream7, DMA_FLAG_TCIF7) == RESET);

    DMA_Cmd(DMA1_Stream7, DISABLE);
    DMA_ClearFlag(DMA1_Stream7, DMA_FLAG_TCIF7);
    TIM_Cmd(TIM3, DISABLE);
}
