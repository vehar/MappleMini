//http://microtechnics.ru/mikrokontroller-stm32-i-usb/
//http://microtechnics.ru/stm32-peredacha-dannyx-po-usb/
//!!! https://myrobot.ru/forum/topic.php?forum=3&topic=501

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"

#include "stdio.h"
#include <string.h>
#include "misc.h"

//------
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"


#define RX_BUF_SIZE 256
volatile char RX_FLAG_END_LINE = 0;
volatile char RXi;
volatile char RXc;
 char RX_BUF[RX_BUF_SIZE] = {'\0'};
 char buffer[RX_BUF_SIZE] = {'\0'};
 
 #define RX_BUF2_SIZE 256
volatile char RX2_FLAG_END_LINE = 0;
volatile char RXi2;
volatile char RXc2;
 char RX_BUF2[RX_BUF2_SIZE] = {'\0'};
 char buffer2[RX_BUF2_SIZE] = {'\0'};
 
 
void clear_RXBuffer(void) {
	for (RXi=0; RXi<RX_BUF_SIZE; RXi++)
		RX_BUF[RXi] = '\0';
	RXi = 0;
}

void clear_RXBuffer2(void) {
	for (RXi2=0; RXi2<RX_BUF2_SIZE; RXi2++)
		RX_BUF2[RXi2] = '\0';
	RXi2 = 0;
}

void usart1_dma_init(void)
{
	/* Enable USART1 and GPIOA clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	//USART1_TX = DMA1_CH4
	//USART1_RX = DMA1_CH5
	
	/* DMA */
	DMA_InitTypeDef DMA_InitStruct;
	DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
	DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)&buffer[0];
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStruct.DMA_BufferSize = sizeof(buffer);
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
	DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel4, &DMA_InitStruct);

	/* NVIC Configuration */
	NVIC_InitTypeDef NVIC_InitStructure;
	/* Enable the USARTx Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure USART1 Tx (PA.09) as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure USART1 Rx (PA.10) as input floating */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure the USART1 */
	USART_InitTypeDef USART_InitStructure;

	/* USART1 configuration ------------------------------------------------------*/
	/* USART1 configured as follow:
		- BaudRate = 115200 baud
		- Word Length = 8 Bits
		- One Stop Bit
		- No parity
		- Hardware flow control disabled (RTS and CTS signals)
		- Receive and transmit enabled
		- USART Clock disabled
		- USART CPOL: Clock is active low
		- USART CPHA: Data is captured on the middle
		- USART LastBit: The clock pulse of the last data bit is not output to
			the SCLK pin
	 */
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART1, &USART_InitStructure);

	/* Enable USART1 */
	USART_Cmd(USART1, ENABLE);

	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	//DMA_Cmd(DMA1_Channel4, ENABLE);

	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);
	NVIC_EnableIRQ(DMA1_Channel4_IRQn);


	/* Enable the USART1 Receive interrupt: this interrupt is generated when the
	USART1 receive data register is not empty */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}



void USART1_SendDMA(char *pucBuffer)
{
	strcpy(buffer, pucBuffer);

	/* Restart DMA Channel*/
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA1_Channel4->CNDTR = strlen(pucBuffer);
	DMA_Cmd(DMA1_Channel4, ENABLE);
}

void DMA1_Channel4_IRQHandler(void)
{
	DMA_ClearITPendingBit(DMA1_IT_TC4);
	DMA_Cmd(DMA1_Channel4, DISABLE);
}

//RM http://www.st.com/content/ccc/resource/technical/document/reference_manual/59/b9/ba/7f/11/af/43/d5/CD00171190.pdf/files/CD00171190.pdf/jcr:content/translations/en.CD00171190.pdf 
void usart2_dma_init(void)
{
	/* Enable USART2 and GPIOA clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB2Periph_GPIOA, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	//USART2_TX = DMA1_CH7
	//USART2_RX = DMA1_CH6
	/* DMA */
	DMA_InitTypeDef DMA_InitStruct;
	DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART2->DR);
	DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)&buffer2[0];
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStruct.DMA_BufferSize = sizeof(buffer2);
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
	DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel7, &DMA_InitStruct);

	/* NVIC Configuration */
	NVIC_InitTypeDef NVIC_InitStructure;
	/* Enable the USARTx Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure USART1 Tx (PA.2) as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure USART1 Rx (PA.3) as input floating */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure the USART2 */
	USART_InitTypeDef USART_InitStructure;

	/* USART2 configuration ------------------------------------------------------*/
	/* USART2 configured as follow:
		- BaudRate = 115200 baud
		- Word Length = 8 Bits
		- One Stop Bit
		- No parity
		- Hardware flow control disabled (RTS and CTS signals)
		- Receive and transmit enabled
		- USART Clock disabled
		- USART CPOL: Clock is active low
		- USART CPHA: Data is captured on the middle
		- USART LastBit: The clock pulse of the last data bit is not output to
			the SCLK pin
	 */
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART2, &USART_InitStructure);

	/* Enable USART2 */
	USART_Cmd(USART2, ENABLE);

	USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
	//DMA_Cmd(DMA1_Channel7, ENABLE);

	DMA_ITConfig(DMA1_Channel7, DMA_IT_TC, ENABLE);
	NVIC_EnableIRQ(DMA1_Channel7_IRQn);


	/* Enable the USART1 Receive interrupt: this interrupt is generated when the
	USART1 receive data register is not empty */
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

void USART1_IRQHandler(void)
{
    if ((USART1->SR & USART_FLAG_RXNE) != (u16)RESET)
	{
    		/*RXc = USART_ReceiveData(USART1);
    		RX_BUF[RXi] = RXc;
    		RXi++;

    		if (RXc != 13) {
    			if (RXi > RX_BUF_SIZE-1) {
    				clear_RXBuffer();
    			}
    		}
    		else {
    			RX_FLAG_END_LINE = 1;
    		}*/

		/* Send the received data to the PC Host*/
    RXc = USART_To_USB_Send_Data(USART1);
		USART_SendData(USART2, RXc);		
		
			//Echo
    		USART_SendData(USART1, RXc);
	}
	
		  /* If overrun condition occurs, clear the ORE flag and recover communication */  
  if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)
  {
    (void)USART_ReceiveData(USART1);
  }
}

void USART2_IRQHandler(void)
{
	
  if ((USART2->SR & USART_FLAG_RXNE) != (u16)RESET)
	{
    		/*RXc2 = USART_ReceiveData(USART2);
    		RX_BUF2[RXi2] = RXc2;
    		RXi2++;

    		if (RXc2 != 13) {
    			if (RXi2 > RX_BUF2_SIZE-1) {
    				clear_RXBuffer2();
    			}
    		}
    		else {
    			RX2_FLAG_END_LINE = 1;
    		}*/
				
    /* Send the received data to the PC Host*/
    RXc2 = USART_To_USB_Send_Data(USART2);
		USART_SendData(USART1, RXc2);		
				
			//Echo
    		USART_SendData(USART2, RXc2);
	}
	
	  /* If overrun condition occurs, clear the ORE flag and recover communication */  
  if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)
  {
    (void)USART_ReceiveData(USART2);
  }
}

void USART2_SendDMA(char *pucBuffer)
{
	strcpy(buffer2, pucBuffer);

	/* Restart DMA Channel*/
	DMA_Cmd(DMA1_Channel7, DISABLE);
	DMA1_Channel7->CNDTR = strlen(pucBuffer);
	DMA_Cmd(DMA1_Channel7, ENABLE);
}

void DMA1_Channel7_IRQHandler(void)
{
	DMA_ClearITPendingBit(DMA1_IT_TC7);
	DMA_Cmd(DMA1_Channel7, DISABLE);
}

uint32_t latency2 = 0;
void USART2_Send(char *pucBuffer)
{
	
    while (*pucBuffer)
    {
			  latency2 = 0;
        USART_SendData(USART2, *pucBuffer++);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET){ latency2++; }
    }
}

volatile short FLAG_ECHO = 0;

void TIM4_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
	{
		FLAG_ECHO = 1;
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}

void usart1_init(void)
{
	    /* Enable USART1 and GPIOA clock */
	    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

	    /* NVIC Configuration */
	    NVIC_InitTypeDef NVIC_InitStructure;
	    /* Enable the USARTx Interrupt */
	    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	    NVIC_Init(&NVIC_InitStructure);


	    /* Configure the GPIOs */
	    GPIO_InitTypeDef GPIO_InitStructure;

	    /* Configure USART1 Tx (PA.09) as alternate function push-pull */
	    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	    GPIO_Init(GPIOA, &GPIO_InitStructure);

	    /* Configure USART1 Rx (PA.10) as input floating */
	    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	    GPIO_Init(GPIOA, &GPIO_InitStructure);

	    /* Configure the USART1 */
	    USART_InitTypeDef USART_InitStructure;

	  /* USART1 configuration ------------------------------------------------------*/
	    /* USART1 configured as follow:
	          - BaudRate = 115200 baud
	          - Word Length = 8 Bits
	          - One Stop Bit
	          - No parity
	          - Hardware flow control disabled (RTS and CTS signals)
	          - Receive and transmit enabled
	          - USART Clock disabled
	          - USART CPOL: Clock is active low
	          - USART CPHA: Data is captured on the middle
	          - USART LastBit: The clock pulse of the last data bit is not output to
	                           the SCLK pin
	    */
	    USART_InitStructure.USART_BaudRate = 115200;
	    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	    USART_InitStructure.USART_StopBits = USART_StopBits_1;
	    USART_InitStructure.USART_Parity = USART_Parity_No;
	    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	    USART_Init(USART1, &USART_InitStructure);

	    /* Enable USART1 */
	    USART_Cmd(USART1, ENABLE);

	    /* Enable the USART1 Receive interrupt: this interrupt is generated when the
	         USART1 receive data register is not empty */
	    //USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

uint32_t latency = 0; //115200 72MHZ = 114

void USART1_Send(char *pucBuffer)
{
	
    while (*pucBuffer)
    {
			  latency = 0;
        USART_SendData(USART1, *pucBuffer++);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET){ latency++; }
    }
}

void SetSysClockTo72(void)
{
	ErrorStatus HSEStartUpStatus;
    /* SYSCLK, HCLK, PCLK2 and PCLK1 configuration -----------------------------*/
    /* RCC system reset(for debug purpose) */
    RCC_DeInit();

    /* Enable HSE */
    RCC_HSEConfig( RCC_HSE_ON);

    /* Wait till HSE is ready */
    HSEStartUpStatus = RCC_WaitForHSEStartUp();

    if (HSEStartUpStatus == SUCCESS)
    {
        /* Enable Prefetch Buffer */
    	//FLASH_PrefetchBufferCmd( FLASH_PrefetchBuffer_Enable);

        /* Flash 2 wait state */
        //FLASH_SetLatency( FLASH_Latency_2);

        /* HCLK = SYSCLK */
        RCC_HCLKConfig( RCC_SYSCLK_Div1);

        /* PCLK2 = HCLK */
        RCC_PCLK2Config( RCC_HCLK_Div1);

        /* PCLK1 = HCLK/2 */
        RCC_PCLK1Config( RCC_HCLK_Div2);

        /* PLLCLK = 8MHz * 9 = 72 MHz */
        RCC_PLLConfig(0x00010000, RCC_PLLMul_9);

        /* Enable PLL */
        RCC_PLLCmd( ENABLE);

        /* Wait till PLL is ready */
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }

        /* Select PLL as system clock source */
        RCC_SYSCLKConfig( RCC_SYSCLKSource_PLLCLK);

        /* Wait till PLL is used as system clock source */
        while (RCC_GetSYSCLKSource() != 0x08)
        {
        }
    }
    else
    { /* If HSE fails to start-up, the application will have wrong clock configuration.
     User can add here some code to deal with this error */

        /* Go to infinite loop */
        while (1)
        {
        }
    }
}

//=================================================================================
volatile uint16_t ADCBuffer[] = {0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA};

void ADC_DMA_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	DMA_InitTypeDef DMA_InitStructure;

	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    /* Enable ADC1 and GPIOA clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 , ENABLE );

	DMA_InitStructure.DMA_BufferSize = 4;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADCBuffer;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	DMA_Cmd(DMA1_Channel1 , ENABLE ) ;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_NbrOfChannel = 4;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_7Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 2, ADC_SampleTime_7Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 3, ADC_SampleTime_7Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 4, ADC_SampleTime_7Cycles5);
	ADC_Cmd(ADC1 , ENABLE ) ;
	ADC_DMACmd(ADC1 , ENABLE ) ;
	ADC_ResetCalibration(ADC1);

	while(ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1);

	while(ADC_GetCalibrationStatus(ADC1));
	ADC_SoftwareStartConvCmd ( ADC1 , ENABLE ) ;
}

void Timer4_init(void)
{
	 // TIMER4
    TIM_TimeBaseInitTypeDef TIMER_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

  	TIM_TimeBaseStructInit(&TIMER_InitStructure);
    TIMER_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIMER_InitStructure.TIM_Prescaler = 7200;
    TIMER_InitStructure.TIM_Period = 5000;
    TIM_TimeBaseInit(TIM4, &TIMER_InitStructure);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM4, ENABLE);

    /* NVIC Configuration */
    /* Enable the TIM4_IRQn Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void Led_init(void)  // Initialize Leds mounted on STM32 MappleMini board
{		
	// Initialize LED which connected to PC13  
	GPIO_InitTypeDef  GPIO_InitStructure;
	// Enable PORTC Clock
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	// Configure the GPIO_LED pin  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOC, GPIO_Pin_13); // Set C13 to Low level ("0")
}

void Led_ON(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

void Led_OFF(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}
//=================================================================================
	//----------------
	extern volatile uint8_t Receive_Buffer[64];
	extern volatile uint32_t Receive_length ;
	extern volatile uint32_t length ;
	uint8_t Send_Buffer[64];
	uint32_t packet_sent=1;
	uint32_t packet_receive=1;

int main(void)
{
	char buffer[80] = {'\0'};

	Set_System(); //---
	SetSysClockTo72();
	
	Led_init();

    //ADC_DMA_init();//ADC
	
    // Initialize USART
    //usart1_init();
	usart1_dma_init();
	usart2_dma_init();
   //Timer4_init();


	Set_USBClock(); //---
	USB_Interrupts_Config(); //---
	USB_Init(); //---
	
		while(1){}
    while (1)
    {

			// from UART 1
			if (RX_FLAG_END_LINE == 1) 
			{
    		// Reset END_LINE Flag
    		RX_FLAG_END_LINE = 0;

    		/* !!! This lines is not have effect. Just a last command USART1_SendDMA(":\r\n"); !!!! */
    		USART1_Send("\r\nI has received a line:\r\n"); // no effect
    		USART1_Send(RX_BUF2); // no effect
    		USART1_Send(":\r\n"); // This command does not wait for the finish of the sending of buffer. It just write to buffer new information and restart sending via DMA.

				USART2_Send(RX_BUF2);
				if (bDeviceState == CONFIGURED)
				{
					CDC_Send_DATA ((uint8_t*)RX_BUF2,RXi2);
				}
		
				/*	if (strncmp(RX_BUF2, "ON\r", 3) == 0) 
				{
    			USART2_Send("THIS IS A COMMAND \"ON\"!!!\r\n");
    			Led_ON();
    		}*/
    		clear_RXBuffer();
    	}
				
			// from UART 2
			 if (RX2_FLAG_END_LINE == 1) 
				{
    		// Reset END_LINE Flag
    		RX2_FLAG_END_LINE = 0;

    		/* !!! This lines is not have effect. Just a last command USART1_SendDMA(":\r\n"); !!!! */
    		USART2_Send("\r\nI has received a line:\r\n"); // no effect
    		USART2_Send(RX_BUF2); // no effect
    		USART2_Send(":\r\n"); // This command does not wait for the finish of the sending of buffer. It just write to buffer new information and restart sending via DMA.

				USART1_Send(RX_BUF2);
				if (bDeviceState == CONFIGURED)
				{
					CDC_Send_DATA ((uint8_t*)RX_BUF2,RXi2);
				}
		
				/*	if (strncmp(RX_BUF2, "ON\r", 3) == 0) 
				{
    			USART2_Send("THIS IS A COMMAND \"ON\"!!!\r\n");
    			Led_ON();
    		}*/
    		clear_RXBuffer2();
    	}
				
			// from USB
				
		
	if (bDeviceState == CONFIGURED)
    {
      CDC_Receive_DATA();
      // Check to see if we have data yet
      if (Receive_length  != 0)
      {
    	 /* // If received symbol '1' then LED turn on, else LED turn off
    	  if (Receive_Buffer[0]=='1') {
    		  GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    	  }
    	  else {
    		  GPIO_SetBits(GPIOC, GPIO_Pin_13);
    	  }*/

    	  // Echo
    	  if (packet_sent == 1) 
				{
    		  CDC_Send_DATA ((uint8_t*)Receive_Buffer,Receive_length);
    	  }

				USART1_Send(Receive_Buffer);
				USART2_Send(Receive_Buffer);
				
    	  Receive_length = 0;
      }
    }
    }
}
