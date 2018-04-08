/**
 * \file            usart.c
 * \brief           OneWire USART application
 */
#include "usart.h"

#define ONEWIRE_USART_USE_DMA                   1

#define ONEWIRE_USART                           USART1
#define ONEWIRE_USART_CLK_EN                    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1)
#define ONEWIRE_USART_RCC_CLOCK                 PCLK2_Frequency

#define ONEWIRE_TX_PORT                         GPIOA
#define ONEWIRE_TX_PORT_CLK_EN                  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA)
#define ONEWIRE_TX_PIN                          LL_GPIO_PIN_9
#define ONEWIRE_TX_PIN_AF                       LL_GPIO_AF_7

#define ONEWIRE_RX_PORT                         GPIOA
#define ONEWIRE_RX_PORT_CLK_EN                  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA)
#define ONEWIRE_RX_PIN                          LL_GPIO_PIN_10
#define ONEWIRE_RX_PIN_AF                       LL_GPIO_AF_7

#define ONEWIRE_USART_TX_DMA                    DMA2
#define ONEWIRE_USART_TX_DMA_CLK_EN             LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2)
#define ONEWIRE_USART_TX_DMA_STREAM             LL_DMA_STREAM_7
#define ONEWIRE_USART_TX_DMA_CHANNEL            LL_DMA_CHANNEL_4
#define ONEWIRE_USART_TX_DMA_CLEAR_FLAGS        do {    \
    LL_DMA_ClearFlag_TC7(ONEWIRE_USART_TX_DMA);         \
    LL_DMA_ClearFlag_HT7(ONEWIRE_USART_TX_DMA);         \
    LL_DMA_ClearFlag_TE7(ONEWIRE_USART_TX_DMA);         \
    LL_DMA_ClearFlag_DME7(ONEWIRE_USART_TX_DMA);        \
    LL_DMA_ClearFlag_FE7(ONEWIRE_USART_TX_DMA);         \
} while (0)

#define ONEWIRE_USART_RX_DMA                    DMA2
#define ONEWIRE_USART_RX_DMA_CLK_EN             LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2)
#define ONEWIRE_USART_RX_DMA_STREAM             LL_DMA_STREAM_2
#define ONEWIRE_USART_RX_DMA_CHANNEL            LL_DMA_CHANNEL_4
#define ONEWIRE_USART_RX_DMA_IRQn               DMA2_Stream2_IRQn
#define ONEWIRE_USART_RX_DMA_IRQ_HANDLER        DMA2_Stream2_IRQHandler
#define ONEWIRE_USART_RX_DMA_CLEAR_FLAGS        do {    \
    LL_DMA_ClearFlag_TC2(ONEWIRE_USART_RX_DMA);         \
    LL_DMA_ClearFlag_HT2(ONEWIRE_USART_RX_DMA);         \
    LL_DMA_ClearFlag_TE2(ONEWIRE_USART_RX_DMA);         \
    LL_DMA_ClearFlag_DME2(ONEWIRE_USART_RX_DMA);        \
    LL_DMA_ClearFlag_FE2(ONEWIRE_USART_RX_DMA);         \
} while (0)

/**
 * \brief           RX completed callback
 * \note            Make it volatile to prevent compiler optimizations
 */
static volatile uint8_t
rx_completed;

/**
 * \brief           UART initialization for OneWire
 * \note            This function must take care of first UART initialization.
 *                  Later, it may be called multiple times to set baudrate
 * \param[in]       baud: Expected baudrate for UART
 */
void
ow_usart_set_baud(uint32_t baud) {
    LL_USART_InitTypeDef USART_InitStruct;
    LL_GPIO_InitTypeDef GPIO_InitStruct;
    
    /*
     * First check if USART is currently enabled.
     *
     * If it is, we assume it was already initialized.
     * In this case, we only need to change baudrate to desired value.
     * We can simply exit from function once finished
     */
    if (LL_USART_IsEnabled(ONEWIRE_USART)) {
        LL_RCC_ClocksTypeDef rcc_clocks;
        
        LL_RCC_GetSystemClocksFreq(&rcc_clocks);/* Read system frequencies */
        LL_USART_Disable(ONEWIRE_USART);        /* First disable USART */
        LL_USART_SetBaudRate(ONEWIRE_USART, rcc_clocks.ONEWIRE_USART_RCC_CLOCK, LL_USART_OVERSAMPLING_16, baud);
        LL_USART_Enable(ONEWIRE_USART);         /* Enable USART back */
        return;
    }

    /* Peripheral clock enable */
    ONEWIRE_TX_PORT_CLK_EN;
    ONEWIRE_RX_PORT_CLK_EN;
    ONEWIRE_USART_TX_DMA_CLK_EN;
    ONEWIRE_USART_RX_DMA_CLK_EN;
    ONEWIRE_USART_CLK_EN;

    /*
     * USART GPIO Configuration
     *
     * USART pins are configured in open-drain mode!
     */
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
    
    /* TX pin */
    GPIO_InitStruct.Pin = ONEWIRE_TX_PIN;
    LL_GPIO_Init(ONEWIRE_TX_PORT, &GPIO_InitStruct);
    /* RX pin */
    GPIO_InitStruct.Pin = ONEWIRE_RX_PIN;
    LL_GPIO_Init(ONEWIRE_RX_PORT, &GPIO_InitStruct);

#if ONEWIRE_USART_USE_DMA
    /* USART RX DMA Init */
    LL_DMA_SetChannelSelection(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, ONEWIRE_USART_RX_DMA_CHANNEL);
    LL_DMA_SetDataTransferDirection(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetStreamPriorityLevel(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM);
    LL_DMA_SetPeriphAddress(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, (uint32_t)&ONEWIRE_USART->DR);

    /* USART TX DMA Init */
    LL_DMA_SetChannelSelection(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, ONEWIRE_USART_TX_DMA_CHANNEL);
    LL_DMA_SetDataTransferDirection(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetStreamPriorityLevel(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM);
    LL_DMA_SetPeriphAddress(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, (uint32_t)&ONEWIRE_USART->DR);
    
    /* Enable DMA RX interrupt */
    NVIC_SetPriority(ONEWIRE_USART_RX_DMA_IRQn, NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 1, 0));
    NVIC_EnableIRQ(ONEWIRE_USART_RX_DMA_IRQn);
#endif /* ONEWIRE_USART_USE_DMA */

    /* Configure UART peripherals */
    USART_InitStruct.BaudRate = 9600;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(ONEWIRE_USART, &USART_InitStruct);
    LL_USART_ConfigAsyncMode(ONEWIRE_USART);
    LL_USART_Enable(ONEWIRE_USART);
}

/**
 * \brief           Transmit-Receive data over OneWire bus
 * \param[in]       tx: Array of data to send
 * \param[out]      rx: Array to save receive data 
 * \param[in]       len: Number of bytes to send
 */
void
ow_usart_tr(const void* tx, void* rx, size_t len) {
#if ONEWIRE_USART_USE_DMA
    
    /* Clear all DMA flags */
    ONEWIRE_USART_RX_DMA_CLEAR_FLAGS;
    ONEWIRE_USART_TX_DMA_CLEAR_FLAGS;
    
    /* Set data length */
    LL_DMA_SetDataLength(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, len);
    LL_DMA_SetDataLength(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, len);
    /* Set memory addresses */
    LL_DMA_SetMemoryAddress(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM, (uint32_t)rx);
    LL_DMA_SetMemoryAddress(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM, (uint32_t)tx);
    
    rx_completed = 0;                           /* Reset RX completed flag */
    
    /* Enable UART DMA requests and start stream */
    LL_USART_EnableDMAReq_RX(ONEWIRE_USART);
    LL_USART_EnableDMAReq_TX(ONEWIRE_USART);
    
    /* Enable transfer complete interrupt */
    LL_DMA_EnableIT_TC(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM);
    
    /* Enable streams and start transfer */
    LL_DMA_EnableStream(ONEWIRE_USART_RX_DMA, ONEWIRE_USART_RX_DMA_STREAM);
    LL_DMA_EnableStream(ONEWIRE_USART_TX_DMA, ONEWIRE_USART_TX_DMA_STREAM);
    
    /*
     * Wait to receive all bytes over UART
     *
     * In case of RTOS usage, use semaphore and put thread
     * in blocked state. Once DMA finishes, use DMA interrupt to unlock thread
     */
    while (!rx_completed) {
        __WFI();                                /* Option for little sleep? ;) */
    }
    
    /* Disable requests */
    LL_USART_DisableDMAReq_RX(ONEWIRE_USART);
    LL_USART_DisableDMAReq_TX(ONEWIRE_USART);
#else /* ONEWIRE_USART_USE_DMA */
    const uint8_t* td = tx;
    uint8_t* rd = rx;
    
    while (len--) {
        /*
         * Step 1: Send byte over UART
         */
        LL_USART_TransmitData8(ONEWIRE_USART, *td++);   /* Write byte to USART port */
        while (!LL_USART_IsActiveFlag_TXE(ONEWIRE_USART));  /* Wait for transmission */
        
        /*
         * Step 2: Wait byte on UART line (loop-back)
         */
        while (!LL_USART_IsActiveFlag_RXNE(ONEWIRE_USART));
        *rd++ = LL_USART_ReceiveData8(ONEWIRE_USART);   /* Get received byte */
    }
#endif /* !ONEWIRE_USART_USE_DMA */
}

/**
 * \brief           USART DMA RX interrupt handler
 * \note            Interrupt is called only when transfer is completed
 */
void
ONEWIRE_USART_RX_DMA_IRQ_HANDLER(void) {
    rx_completed = 1;                           /* Set RX completed flag */
    ONEWIRE_USART_RX_DMA_CLEAR_FLAGS;           /* Clear all flags */
}
