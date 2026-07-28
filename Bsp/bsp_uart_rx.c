/**
 ******************************************************************************
 * @file    bsp_uart_rx.c
 * @brief   USART1 RX DMA + IDLE line detection implementation
 ******************************************************************************
 */

#include "bsp_uart_rx.h"

/* ---- Extern handles from CubeMX ---- */
extern UART_HandleTypeDef huart1;

/* ---- DMA handle for USART1_RX — non-static for ISR access ---- */
DMA_HandleTypeDef hdma_usart1_rx;

/* ---- Circular DMA buffer ---- */
static uint8_t dmabuf[UART_RX_BUF_SIZE] __attribute__((aligned(32)));
static volatile uint16_t dma_last_ndtr = UART_RX_BUF_SIZE;// Number of Data to Transfer

/* ---- Ring buffer for main-loop consumption ---- */
static uint8_t  rx_ring[UART_RX_FRAME_MAX]; //256
static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static volatile bool     rx_overflow = false;

/* ---- Forward declarations ---- */
static void dma_restart(void);

/* ================================================================
 * DMA init for USART1_RX (DMA1 Stream 0, DMAMUX Req 40)
 * ================================================================ */

static void DMA_USART1_RX_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    hdma_usart1_rx.Instance = DMA1_Stream0;
    hdma_usart1_rx.Init.Request = 40U;          /* DMAMUX1: USART1_RX */
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma_usart1_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_usart1_rx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_usart1_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;

    HAL_DMA_Init(&hdma_usart1_rx);

    /* Associate with USART1 */
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    /* NVIC: DMA1_Stream0 */
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

/* ================================================================
 * Public API
 * ================================================================ */

void BSP_UartRx_Init(UART_HandleTypeDef *huart)
{
    (void)huart;

    DMA_USART1_RX_Init();

    /* Enable USART1 IDLE interrupt */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    /* NVIC: USART1 */
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* Start DMA circular receive */
    dma_restart();
}

// ISR中断服务例程（Interrupt Service Routine）
void BSP_UartRx_IdleISR(void)
{
    uint16_t ndtr = __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    uint16_t new_bytes;

    if (ndtr <= dma_last_ndtr) {
        new_bytes = dma_last_ndtr - ndtr;
    } else {
        /* DMA wrapped around */
        new_bytes = (UART_RX_BUF_SIZE - dma_last_ndtr) + ndtr;
    }

    if (new_bytes == 0U) {
        return;
    }

    /* Copy into ring buffer */
    uint16_t write_idx = (UART_RX_BUF_SIZE - dma_last_ndtr) % UART_RX_BUF_SIZE;
    for (uint16_t i = 0U; i < new_bytes; i++) {
        if (((rx_head + 1U) % UART_RX_FRAME_MAX) == rx_tail) {
            rx_overflow = true;
            break;
        }
        rx_ring[rx_head] = dmabuf[write_idx];
        write_idx = (write_idx + 1U) % UART_RX_BUF_SIZE;
        rx_head = (rx_head + 1U) % UART_RX_FRAME_MAX;
    }

    /* Restart DMA for next frame */
    dma_restart();
}

bool BSP_UartRx_Available(void)
{
    return (rx_tail != rx_head);
}

bool BSP_UartRx_ReadByte(uint8_t *byte)
{
    if (rx_tail == rx_head) {
        return false;
    }
    *byte = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1U) % UART_RX_FRAME_MAX;
    return true;
}

void BSP_UartRx_Flush(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow = false;
    dma_restart();
}

uint16_t BSP_UartRx_Count(void)
{
    if (rx_head >= rx_tail) {
        return rx_head - rx_tail;
    }
    return (UART_RX_FRAME_MAX - rx_tail) + rx_head;
}

/* ================================================================
 * Internal
 * ================================================================ */

static void dma_restart(void)
{
    HAL_DMA_Abort(&hdma_usart1_rx);
    dma_last_ndtr = UART_RX_BUF_SIZE;
    HAL_DMA_Start(&hdma_usart1_rx,
                  (uint32_t)&USART1->RDR,
                  (uint32_t)dmabuf,
                  UART_RX_BUF_SIZE);
}
