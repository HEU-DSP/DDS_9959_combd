/**
 ******************************************************************************
 * @file    bsp_uart_rx.c
 * @brief   USART1 RX via HAL_UARTEx_ReceiveToIdle_DMA + ring buffer.
 *
 * HAL handles DMAR bit, DMAMUX channel mapping, DMA config, and IDLE
 * detection through HAL_UARTEx_RxEventCallback.
 * Received data is copied to a ring buffer for main-loop consumption.
 ******************************************************************************
 */

#include "bsp_uart_rx.h"

extern UART_HandleTypeDef huart1;

/* ---- DMA handle (DMA1 Stream 0, DMAMUX = USART1_RX) ---- */
static DMA_HandleTypeDef hdma_usart1_rx;

/*
 * This buffer is accessed by DMA.  Keep it in the linker's DMA section
 * (RAM_D2), rather than the default DTCM .bss section.
 */
static uint8_t dmabuf[UART_RX_BUF_SIZE]
    __attribute__((section(".dma_buffer"), aligned(32)));
static volatile uint16_t dma_last_pos = 0U;

/* ---- Ring buffer for main loop ---- */
static uint8_t  rx_ring[UART_RX_FRAME_MAX];
static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static volatile bool     rx_overflow = false;

/* ---- Debug: Ozone-observable ---- */
volatile uint32_t uart_rx_diag[6] = {0};
/* ---- Forward ---- */
static void rx_start(void);
static void copy_to_ring(const uint8_t *src, uint16_t len);
static void copy_dma_new_data(uint16_t dma_pos);

/* ================================================================
 * HAL UART RX Event Callback (ISR context, on IDLE)
 * ================================================================ */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1) return;

    /* In circular mode Size is the current write position.  Do not restart
     * DMA here: restarting is the race that previously left NDTR at 255. */
    if (Size <= UART_RX_BUF_SIZE) {
        copy_dma_new_data(Size);
    }
    uart_rx_diag[5]++;
}

/* ================================================================
 * Public API
 * ================================================================ */

void BSP_UartRx_Init(UART_HandleTypeDef *huart)
{
    (void)huart;

    /* ---- DMA Init: DMA1 Stream 0, DMAMUX request = USART1_RX ---- */
    __HAL_RCC_DMA1_CLK_ENABLE();
    RCC->AHB1ENR |= (1UL << 2U);  /* DMAMUX1 clock */

    hdma_usart1_rx.Instance                 = DMA1_Stream0;
    hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart1_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart1_rx);

    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    /* NVIC */
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* Start once.  It remains armed across every IDLE event. */
    dma_last_pos = 0U;
    rx_start();
}

void BSP_UartRx_IdleISR(void)
{
    /* Not used: HAL_UARTEx_RxEventCallback handles IDLE.
     * Stub retained for it.c compatibility. */
}

bool BSP_UartRx_Available(void)
{
    return (rx_tail != rx_head);
}

bool BSP_UartRx_ReadByte(uint8_t *byte)
{
    if (rx_tail == rx_head) return false;
    *byte = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1U) % UART_RX_FRAME_MAX;
    return true;
}

void BSP_UartRx_Flush(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow = false;
    dma_last_pos = 0U;
}

uint16_t BSP_UartRx_Count(void)
{
    if (rx_head >= rx_tail) return rx_head - rx_tail;
    return (UART_RX_FRAME_MAX - rx_tail) + rx_head;
}

/* ================================================================
 * Internal
 * ================================================================ */

static void copy_to_ring(const uint8_t *src, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++) {
        if (((rx_head + 1U) % UART_RX_FRAME_MAX) == rx_tail) {
            rx_overflow = true;
            break;
        }
        rx_ring[rx_head] = src[i];
        rx_head = (rx_head + 1U) % UART_RX_FRAME_MAX;
    }
}

static void rx_start(void)
{
    uart_rx_diag[3] = (uint32_t)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dmabuf, UART_RX_BUF_SIZE);
    uart_rx_diag[0] = (uint32_t)huart1.RxState;
    uart_rx_diag[1] = (uint32_t)huart1.RxXferCount;
    uart_rx_diag[2] = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
}

/* Copy bytes written by circular DMA since the prior callback. */
static void copy_dma_new_data(uint16_t dma_pos)
{
    if (dma_pos == dma_last_pos) return;

    if (dma_pos > dma_last_pos) {
        copy_to_ring(&dmabuf[dma_last_pos], dma_pos - dma_last_pos);
    } else {
        copy_to_ring(&dmabuf[dma_last_pos], UART_RX_BUF_SIZE - dma_last_pos);
        if (dma_pos > 0U) copy_to_ring(&dmabuf[0], dma_pos);
    }

    /* HAL reports Size=buffer-size on a DMA transfer-complete event.  The
     * hardware has wrapped back to index zero for subsequent writes. */
    dma_last_pos = (dma_pos == UART_RX_BUF_SIZE) ? 0U : dma_pos;
}
