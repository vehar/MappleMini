#include "nRF24L01.h"
#include "stm32f10x.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"

u8 TxBuf[32]={0};
unsigned int sta;

// Predefine TX payload packet..
unsigned char const TX_PAYLOAD[TX_PLOAD_WIDTH] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                                         0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};

unsigned char SPI_Buffer[32];                       // Buffer to hold data from 'SPI_Read_Buf()' function


unsigned char TX_ADDRESS[TX_ADR_WIDTH]  = {0x34,0x43,0x10,0x10,0x01}; // Define a static TX address
unsigned char rx_buf[TX_PLOAD_WIDTH];
unsigned char tx_buf[TX_PLOAD_WIDTH];
unsigned char flag;
unsigned char rx_com_buffer[10];
unsigned char tx_com_buffer[10];
//unsigned char i;
unsigned char accept_flag;
// SPI(nRF24L01) commands


#define RF_READ_REG    0x00  // Define read command to register
#define RF_WRITE_REG   0x20  // Define write command to register
#define RD_RX_PLOAD 0x61  // Define RX payload register address
#define WR_TX_PLOAD 0xA0  // Define TX payload register address
#define FLUSH_TX    0xE1  // Define flush TX register command
#define FLUSH_RX    0xE2  // Define flush RX register command
#define REUSE_TX_PL 0xE3  // Define reuse TX payload register command
#define NOP         0xFF  // Define No Operation, might be used to read status register


#define  RX_DR  ((sta>>6)&0X01)
#define  TX_DS  ((sta>>5)&0X01)
#define  MAX_RT  ((sta>>4)&0X01)


//Chip Enable Activates RX or TX mode
#define CE_H()   GPIO_SetBits(GPIOB, GPIO_Pin_10)
#define CE_L()   GPIO_ResetBits(GPIOB, GPIO_Pin_8)


//SPI Chip Select
#define CSN_H()  GPIO_SetBits(GPIOB, GPIO_Pin_1)//GPIO_Pin_12
#define CSN_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_1)//GPIO_Pin_12

extern void nRF24_HardInit(void);
extern void Delay_ms(unsigned int  n);
extern void Delay_us(unsigned int  n);

void SPI2_Init(void)
{
	nRF24_HardInit();
        /*SPI_InitTypeDef   SPI_InitStructure;
	GPIO_InitTypeDef      GPIO_InitStructure;
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); //enable spi2 clock.
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 |GPIO_Pin_14 |GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure); 
        //CE
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
        //IRQ
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


         SPI_Cmd(SPI2, DISABLE); 
        // SPI1 configuration //
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;// 
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI2, &SPI_InitStructure);
	// Enable SPI2  //
	SPI_Cmd(SPI2, ENABLE);   */

}



/*******************************************************************************
* Function Name   : SPI2_RW
* Description : Sends a byte through the SPI interface and return the byte
*                received from the SPI bus.
* Input       : byte : byte to send.
* Output       : None
* Return       : The value of the received byte.
*******************************************************************************/
u8 SPI2_readWrite(u8 byte)
{
	/* Loop while DR register in not emplty */
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
	/* Send byte through the SPI1 peripheral */
	SPI_I2S_SendData(SPI2, byte);
	/* Wait to receive a byte */
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
	/* Return the byte read from the SPI bus */
	return SPI_I2S_ReceiveData(SPI2);
}




unsigned char SPI2_readWriteReg(unsigned char reg, unsigned char value)
{
        unsigned char status;
        CSN_L();
        // select register 
        status = SPI2_readWrite(reg);
        // set value
        SPI2_readWrite(value);
        CSN_H();
        return(status);
}


unsigned char SPI2_readReg(unsigned char reg)
{
        unsigned char reg_val;
	CSN_L();
	SPI2_readWrite(reg);
	reg_val = SPI2_readWrite(0);
	CSN_H();
	return(reg_val);
}


unsigned char SPI2_readBuf(unsigned char reg,unsigned char *pBuf, unsigned char bytes)
{
        unsigned char status,i;
        CSN_L();
        // Select register to write to and read status byte
        status = SPI2_readWrite(reg);
        for(i=0;i<bytes;i++)
			pBuf[i] = SPI2_readWrite(0);
        CSN_H();
        return(status);
}


unsigned char SPI2_writeBuf(unsigned char reg, unsigned char *pBuf, unsigned char bytes)
{
        unsigned char status,i;
        CSN_L();
        // Select register to write to and read status byte
        status = SPI2_readWrite(reg);
        for(i=0; i<bytes; i++) // then write all byte in buffer(*pBuf)
        	SPI2_readWrite(*pBuf++);
        CSN_H();
        return(status);
}


void RX_Mode(void)
{
        CE_L();
        SPI2_writeBuf(NRF_WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH);
        SPI2_readWriteReg(NRF_WRITE_REG + EN_AA, 0x01);      // Enable Auto.Ack:Pipe0
        SPI2_readWriteReg(NRF_WRITE_REG + EN_RXADDR, 0x01); // Enable Pipe0
        SPI2_readWriteReg(NRF_WRITE_REG + RF_CH, 40);       // Select RF channel 40
        SPI2_readWriteReg(NRF_WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH); // Select same RX payload width as TX Payload width
        SPI2_readWriteReg(NRF_WRITE_REG + RF_SETUP, 0x07/*0x0f*/); // TX_PWR:0dBm, Datarate:2Mbps, LNA:HCURR
        SPI2_readWriteReg(NRF_WRITE_REG + CONFIG, 0x0f);       // Set PWR_UP bit, enable CRC(2 bytes)
        //& Prim:RX. RX_DR enabled..
        CE_H(); // Set CE pin high to enable RX device

        // This device is now ready to receive one packet of 16 bytes payload from a TX device
        //sending to address
        // '3443101001', with auto acknowledgment, retransmit count of 10, RF channel 40 and
        //datarate = 2Mbps.
}


void TX_Mode(unsigned char * BUF)
{
		CE_L();//CE=0


        SPI2_readWriteReg(NRF_WRITE_REG + EN_AA, 0x01);     // Enable Auto.Ack:Pipe0
        SPI2_readWriteReg(NRF_WRITE_REG + EN_RXADDR, 0x01); // Enable Pipe0
        SPI2_readWriteReg(NRF_WRITE_REG + SETUP_RETR, 0x1a); // 500us + 86us, 10 retrans...
        SPI2_readWriteReg(NRF_WRITE_REG + RF_CH, 40);        // Select RF channel 40
        SPI2_readWriteReg(NRF_WRITE_REG + RF_SETUP, 0x07/*0x0f*/);   // TX_PWR:0dBm, Datarate:2Mbps, LNA:HCURR
        SPI2_readWriteReg(NRF_WRITE_REG + CONFIG, 0x0e);     // Set PWR_UP bit, enable CRC(2 bytes)& Prim:TX. MAX_RT & TX_DS enabled..

        SPI2_writeBuf(NRF_WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH); // Writes TX_Address to nRF24L01
        SPI2_writeBuf(NRF_WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH); // RX_Addr0 same as TX_Adr for Auto.Ack
        SPI2_writeBuf(WR_TX_PLOAD, BUF /*TX_PAYLOAD*/, TX_PLOAD_WIDTH); // Writes data to TX payload

        //  This device is now ready to transmit one packet of 16 bytes payload to a RX device at address
        //  '3443101001', with auto acknowledgment, retransmit count of 10(retransmit delay of 500µs+86µs)
        //  RF channel 40, datarate = 2Mbps with TX power = 0dBm.
       // CE_H();
       //delay_us(15);
        CE_L(); // Set CE pin high
}
/*
void nRF24L01_IRQ(void) interrupt EXT_INT0
{
BYTE temp,rx_pw;

  EA = 0; // disable global interrupt during processing
  temp = SPI_RW_Reg(WRITE_REG + STATUS, 0x70);    // Read STATUS byte and clear IRQ flag's(nRF24L01)

  if(temp & MAX_RT) IRQ_Source = MAX_RT;          // Indicates max #of retransmit interrupt
  if(temp & TX_DS)  IRQ_Source = TX_DS;           // Indicates TX data succsessfully sent

  if(temp & RX_DR)  // In RX mode, check for data received
  {
    // Data received, so find out which datapipe the data was received on:
    temp = (0x07 & (temp > 1)); // Shift bits in status byte one bit to LSB and mask 'Data Pipe Number'
    rx_pw = SPI_Read(READ_REG + RX_PW_P0 + temp); // Read current RX_PW_Pn register, where Pn is the pipe the data was received on..
    SPI_Read_Buf(RD_RX_PLOAD, SPI_Buffer, rx_pw); // Data from RX Payload register is now copied to SPI_Buffer[].

    IRQ_Source = RX_DR; // Indicates RX data received
  }
  EA = 1; // enable global interrupt again
}
*/
void init_NRF24L01(void)
{
	u8 buf[5]={0};
 
	Delay_us(100);
	CE_L();    // chip enable
	//CSN_H();   // Spi disable 
	SPI2_readBuf(TX_ADDR, buf, TX_ADR_WIDTH);
}

void init_io(void)
{
	CE_L();
	CSN_H();
	RX_Mode();
}

