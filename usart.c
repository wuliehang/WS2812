#include "sys.h"
#include "usart.h"	

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 
}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{ 	
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
	USART1->DR = (u8) ch;      
	return ch;
}
#endif
 
//串口1中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误   	
u8 USART_RX_BUF[USART_REC_LEN];     //接收缓冲,最大USART_REC_LEN个字节.
//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
u16 USART_RX_STA=0;       //接收状态标记	

//初始化IO 串口1 
//bound:波特率
void uart_init(u32 bound)
{
GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_DMA2, ENABLE); // PA+DMA2
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // GPIO复用为串口
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);  // TX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1); // RX

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART 配置
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    // DMA2 Stream2 通道4 配置为 USART1_RX
    DMA_DeInit(DMA2_Stream2);
    while (DMA_GetCmdStatus(DMA2_Stream2) != DISABLE);

    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&USART1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)USART_RX_BUF;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = USART_REC_LEN;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;  // 循环模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream2, &DMA_InitStructure);
    DMA_Cmd(DMA2_Stream2, ENABLE);

    // 使能DMA接收请求
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    // 开启USART空闲中断
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    // USART1中断配置
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

	
}


void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
    {
        volatile uint32_t temp;
        temp = USART1->SR;
        temp = USART1->DR;

        DMA_Cmd(DMA2_Stream2, DISABLE);

        uint16_t len = USART_REC_LEN - DMA_GetCurrDataCounter(DMA2_Stream2);

        // 判断是否以 0x0D 0x0A 结尾
        if (len >= 2 &&
            USART_RX_BUF[len - 2] == 0x0D &&
            USART_RX_BUF[len - 1] == 0x0A)
        {
            // ? 使用原有变量，设置接收完成标志和长度
            USART_RX_STA = 0;
            USART_RX_STA |= 0x8000;       // 接收完成标志
            USART_RX_STA |= len & 0x3FFF; // 长度
        }
        else
        {
            // 帧尾错误，忽略本次
            USART_RX_STA = 0;
        }

        // 重新配置 DMA
        DMA_SetCurrDataCounter(DMA2_Stream2, USART_REC_LEN);
        DMA_Cmd(DMA2_Stream2, ENABLE);
    }
}
 



