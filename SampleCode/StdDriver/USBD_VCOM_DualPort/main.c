/****************************************************************************//**
 * @file     main.c
 * @version  V3.00
 * @brief    Demonstrate how to implement a USB dual virtual COM port device.(UART + UUART)
 *
 * @copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NUC121.h"
#include "cdc_serial.h"

/*--------------------------------------------------------------------------*/
STR_VCOM_LINE_CODING gLineCoding0 = {115200, 0, 0, 8};   /* UART0: Baud rate : 115200    */
STR_VCOM_LINE_CODING gLineCoding1 = {115200, 0, 0, 8};   /* USCI0_UART(UUART):Baud rate : 115200 */
/* Stop bit     */
/* parity       */
/* data bits    */
uint16_t gCtrlSignal0 = 0;     /* BIT0: DTR(Data Terminal Ready) , BIT1: RTS(Request To Send) */
uint16_t gCtrlSignal1 = 0;     /* BIT0: DTR(Data Terminal Ready) , BIT1: RTS(Request To Send) */

/*--------------------------------------------------------------------------*/
#define RXBUFSIZE           512 /* RX buffer size */
#define TXBUFSIZE           512 /* RX buffer size */

#define TX_FIFO_SIZE_0      64  /* UART: TX Hardware FIFO size */
#define TX_FIFO_SIZE_1      64  /* USCI_UART(UUART): TX Hardware FIFO size */

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
/* UART0 */
volatile uint8_t comRbuf0[RXBUFSIZE];
volatile uint16_t comRbytes0 = 0;
volatile uint16_t comRhead0 = 0;
volatile uint16_t comRtail0 = 0;

volatile uint8_t comTbuf0[TXBUFSIZE];
volatile uint16_t comTbytes0 = 0;
volatile uint16_t comThead0 = 0;
volatile uint16_t comTtail0 = 0;

uint8_t gRxBuf0[64] = {0};
volatile uint8_t *gpu8RxBuf0 = 0;
volatile uint32_t gu32RxSize0 = 0;
volatile uint32_t gu32TxSize0 = 0;

volatile int8_t gi8BulkOutReady0 = 0;

/* UART1 */
volatile uint8_t comRbuf1[RXBUFSIZE];
volatile uint16_t comRbytes1 = 0;
volatile uint16_t comRhead1 = 0;
volatile uint16_t comRtail1 = 0;

volatile uint8_t comTbuf1[TXBUFSIZE];
volatile uint16_t comTbytes1 = 0;
volatile uint16_t comThead1 = 0;
volatile uint16_t comTtail1 = 0;

uint8_t gRxBuf1[64] = {0};
volatile uint8_t *gpu8RxBuf1 = 0;
volatile uint32_t gu32RxSize1 = 0;
volatile uint32_t gu32TxSize1 = 0;

volatile int8_t gi8BulkOutReady1 = 0;


/*--------------------------------------------------------------------------*/
void SYS_Init(void)
{

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable Internal HIRC 48 MHz clock */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN);

    /* Waiting for Internal HIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    /* Switch HCLK clock source to Internal HIRC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /* Enable module clock */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(USBD_MODULE);
    CLK_EnableModuleClock(USCI0_MODULE);

    /* Select module clock source */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HIRC_DIV2, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(USBD_MODULE, CLK_CLKSEL3_USBDSEL_HIRC, CLK_CLKDIV0_USB(1));


    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Set PB multi-function pins for UART0 RXD and TXD */
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB0MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk);
    SYS->GPB_MFPL = SYS_GPB_MFPL_PB0MFP_UART0_RXD | SYS_GPB_MFPL_PB1MFP_UART0_TXD;

    /* Set PB multi-function pins for USCI0_DAT0(PB.4) and USCI0_DAT1(PB.5) */
    SYS->GPB_MFPL = (SYS->GPB_MFPL & (~SYS_GPB_MFPL_PB4MFP_Msk)) | SYS_GPB_MFPL_PB4MFP_USCI0_DAT0;
    SYS->GPB_MFPL = (SYS->GPB_MFPL & (~SYS_GPB_MFPL_PB5MFP_Msk)) | SYS_GPB_MFPL_PB5MFP_USCI0_DAT1;

}


void UART0_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Reset IP */
    SYS->IPRST1 |=  SYS_IPRST1_UART0RST_Msk;
    SYS->IPRST1 &= ~SYS_IPRST1_UART0RST_Msk;

    /* Configure UART0 and set UART0 Baudrate */
    UART0->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HIRC_DIV2, 115200);
    UART0->LINE = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;

    UART0->INTEN = UART_INTEN_TOCNTEN_Msk | UART_INTEN_RDAIEN_Msk;
}



void USCI0_Init()
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init USCI                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Reset USCI0 */
    SYS_ResetModule(USCI0_RST);

    /* Configure USCI0 as UART mode */
    UUART_Open(UUART0, 115200);
}



/*---------------------------------------------------------------------------------------------------------*/
/* UART Callback function                                                                                  */
/*---------------------------------------------------------------------------------------------------------*/
void UART0_IRQHandler(void)
{
    uint32_t u32IntStatus;
    uint8_t bInChar;
    int32_t size;

    u32IntStatus = UART0->INTSTS;

    if ((u32IntStatus & UART_INTSTS_RDAIF_Msk) || (u32IntStatus & UART_INTSTS_RXTOIF_Msk)) {
        /* Receiver FIFO threshold level is reached or Rx time out */

        /* Get all the input characters */
        while ((UART0->FIFOSTS & UART_FIFOSTS_RXEMPTY_Msk) == 0) {
            /* Get the character from UART Buffer */
            bInChar = UART0->DAT;

            /* Check if buffer full */
            if (comRbytes0 < RXBUFSIZE) {
                /* Enqueue the character */
                comRbuf0[comRtail0++] = bInChar;

                if (comRtail0 >= RXBUFSIZE)
                    comRtail0 = 0;

                comRbytes0++;
            } else {
                /* FIFO over run */
            }
        }
    }

    if (u32IntStatus & UART_INTSTS_THREIF_Msk) {

        if (comTbytes0) {
            /* Fill the Tx FIFO */
            size = comTbytes0;

            if (size >= TX_FIFO_SIZE_0) {
                size = TX_FIFO_SIZE_0;
            }

            while (size) {
                bInChar = comTbuf0[comThead0++];
                UART0->DAT = bInChar;

                if (comThead0 >= TXBUFSIZE)
                    comThead0 = 0;

                comTbytes0--;
                size--;
            }
        } else {
            /* No more data, just stop Tx (Stop work) */
            UART0->INTEN &= (~UART_INTEN_THREIEN_Msk);
        }
    }

}








void USCI_IRQHandler(void)
{
    uint32_t u32IntStatus;
    uint8_t bInChar;

    u32IntStatus = UUART0->PROTSTS;

    if (u32IntStatus & UUART_PROTSTS_RXENDIF_Msk) {
        /* Cleare RX end interrupt flag */
        UUART_CLR_PROT_INT_FLAG(UUART0, UUART_PROTSTS_RXENDIF_Msk);

        /* Get all the input characters */
        while (!UUART_IS_RX_EMPTY(UUART0)) {
            /* Get the character from UART Buffer */
            bInChar = UUART_READ(UUART0);

            /* Check if buffer full */
            if (comRbytes1 < RXBUFSIZE) {
                /* Enqueue the character */
                comRbuf1[comRtail1++] = bInChar;

                if (comRtail1 >= RXBUFSIZE)
                    comRtail1 = 0;

                comRbytes1++;
            } else {
                /* FIFO over run */
            }
        }
    }

}

void VCOM_TransferData(void)
{
    int32_t i, i32Len;

    /* Check whether USB is ready for next packet or not */
    if (gu32TxSize0 == 0) {
        /* Check whether we have new COM Rx data to send to USB or not */
        if (comRbytes0) {
            i32Len = comRbytes0;

            if (i32Len > EP2_MAX_PKT_SIZE)
                i32Len = EP2_MAX_PKT_SIZE;

            for (i = 0; i < i32Len; i++) {
                gRxBuf0[i] = comRbuf0[comRhead0++];

                if (comRhead0 >= RXBUFSIZE)
                    comRhead0 = 0;
            }

            __set_PRIMASK(1);
            comRbytes0 -= i32Len;
            __set_PRIMASK(0);

            gu32TxSize0 = i32Len;
            USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2)), (uint8_t *)gRxBuf0, i32Len);
            USBD_SET_PAYLOAD_LEN(EP2, i32Len);
        } else {
            /* Prepare a zero packet if previous packet size is EP2_MAX_PKT_SIZE and
               no more data to send at this moment to note Host the transfer has been done */
            i32Len = USBD_GET_PAYLOAD_LEN(EP2);

            if (i32Len == EP2_MAX_PKT_SIZE)
                USBD_SET_PAYLOAD_LEN(EP2, 0);
        }
    }


    if (gu32TxSize1 == 0) {
        /* Check whether we have new COM Rx data to send to USB or not */
        if (comRbytes1) {
            i32Len = comRbytes1;

            if (i32Len > EP7_MAX_PKT_SIZE)
                i32Len = EP7_MAX_PKT_SIZE;

            for (i = 0; i < i32Len; i++) {
                gRxBuf1[i] = comRbuf1[comRhead1++];

                if (comRhead1 >= RXBUFSIZE)
                    comRhead1 = 0;
            }

            __set_PRIMASK(1);
            comRbytes1 -= i32Len;
            __set_PRIMASK(0);

            gu32TxSize1 = i32Len;
            USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP7)), (uint8_t *)gRxBuf1, i32Len);
            USBD_SET_PAYLOAD_LEN(EP7, i32Len);
        } else {
            /* Prepare a zero packet if previous packet size is EP7_MAX_PKT_SIZE and
               no more data to send at this moment to note Host the transfer has been done */
            i32Len = USBD_GET_PAYLOAD_LEN(EP7);

            if (i32Len == EP7_MAX_PKT_SIZE)
                USBD_SET_PAYLOAD_LEN(EP7, 0);
        }
    }

    /* Process the Bulk out data when bulk out data is ready. */
    if (gi8BulkOutReady0 && (gu32RxSize0 <= TXBUFSIZE - comTbytes0)) {
        for (i = 0; i < gu32RxSize0; i++) {
            comTbuf0[comTtail0++] = gpu8RxBuf0[i];

            if (comTtail0 >= TXBUFSIZE)
                comTtail0 = 0;
        }

        __set_PRIMASK(1);
        comTbytes0 += gu32RxSize0;
        __set_PRIMASK(0);

        gu32RxSize0 = 0;
        gi8BulkOutReady0 = 0; /* Clear bulk out ready flag */

        /* Ready to get next BULK out */
        USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
    }

    if (gi8BulkOutReady1 && (gu32RxSize1 <= TXBUFSIZE - comTbytes1)) {
        for (i = 0; i < gu32RxSize1; i++) {
            comTbuf1[comTtail1++] = gpu8RxBuf1[i];

            if (comTtail1 >= TXBUFSIZE)
                comTtail1 = 0;
        }

        __set_PRIMASK(1);
        comTbytes1 += gu32RxSize1;
        __set_PRIMASK(0);

        gu32RxSize1 = 0;
        gi8BulkOutReady1 = 0; /* Clear bulk out ready flag */

        /* Ready to get next BULK out */
        USBD_SET_PAYLOAD_LEN(EP6, EP6_MAX_PKT_SIZE);
    }

    /* Process the software Tx FIFO */
    if (comTbytes0) {
        /* Check if Tx is working */
        if ((UART0->INTEN & UART_INTEN_THREIEN_Msk) == 0) {
            /* Send one bytes out */
            UART0->DAT = comTbuf0[comThead0++];

            if (comThead0 >= TXBUFSIZE)
                comThead0 = 0;

            __set_PRIMASK(1);
            comTbytes0--;
            __set_PRIMASK(0);

            /* Enable Tx Empty Interrupt. (Trigger first one) */
            UART0->INTEN |= UART_INTEN_THREIEN_Msk;
        }
    }



    /* Check if data received from USB_OUT are not been transfered to USCI_UART(UUART)*/
    if (comTbytes1) {
        uint32_t size;
        uint8_t bInChar;

        size = comTbytes1;

        while (size--) {
            while (UUART_IS_TX_FULL(UUART0)); /* Wait Tx is not full to transmit data */

            /* Send one bytes out */
            bInChar = comTbuf1[comThead1++];
            UUART_WRITE(UUART0, bInChar);

            if (comThead1 >= TXBUFSIZE)
                comThead1 = 0;

            __set_PRIMASK(1);
            comTbytes1--;
            __set_PRIMASK(0);
        }
    }


}


/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    SYS_Init();
    UART0_Init();

    //NUC121 is with UART0 only so we use UUART for alternative
    USCI0_Init();

    printf("\n\n");
    printf("+---------------------------------------------------------------+\n");
    printf("| NuMicro USB Virtual COM Dual Port Sample Code(UART,USCI_UART) |\n");
    printf("+---------------------------------------------------------------+\n");
    printf("Set PB.0 as UART RX pin and PB.1 as UART TX pin\n");
    printf("Set PB.4 as UUART RX pin and PB.5 as UUART TX pin\n");

    /* Open USB controller */
    USBD_Open(&gsInfo, VCOM_ClassRequest, NULL);

    /* Endpoint configuration */
    VCOM_Init();
    /* Start USB device */
    USBD_Start();
    NVIC_EnableIRQ(USBD_IRQn);
    NVIC_EnableIRQ(UART0_IRQn);


    /* Enable USCI UART receive and transmit end interrupt */
    UUART_ENABLE_TRANS_INT(UUART0, UUART_INTEN_RXENDIEN_Msk);
    NVIC_EnableIRQ(USCI_IRQn);

    while (1) {
        VCOM_TransferData();
    }
}



/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/

