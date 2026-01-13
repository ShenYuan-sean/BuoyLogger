//
// Created by link on 2025/12/5.
//

#include "main.h"
#include "usart.h"
#include "iwdg.h"
#include "app.h"
#include <string.h>

#define UART1_RX_BUFF_SIZE 1024
#define UART2_RX_BUFF_SIZE 1024
#define UART3_RX_BUFF_SIZE 1024

volatile uint8_t uart1_rx_flag = 0;  // 信标数据接收标志位
volatile uint16_t uart1_rx_len = 0;
uint8_t uart1_rx_buf[UART1_RX_BUFF_SIZE];

volatile uint8_t uart2_rx_flag = 0;  // 高精度时钟
volatile uint16_t uart2_rx_len = 0;
uint8_t uart2_rx_buf[UART2_RX_BUFF_SIZE];

volatile uint8_t uart3_rx_flag = 0;  //rtk 接收标志位
volatile uint16_t uart3_rx_len = 0;
uint8_t uart3_rx_buf[UART3_RX_BUFF_SIZE]; // DMA 接收缓冲



// 用于喂看门狗，保证收到两种数据
volatile uint8_t got_hhrng = 0;
volatile uint8_t got_txt = 0;
volatile uint8_t got_gga = 0;



// 10s
void feed_watchdog(void)
{
    if(HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
    {
        /* Refresh Error */
        Error_Handler();
    }
}

/* 提取收到数据中的第一句GGA的那一句，丢弃其他的
$GNGGA,000104.900,,,,,0,00,99.99,,M,,M,,*44\r\n
$GNRMC,000104.900,V,,,,,,,060180,,,N,V*2A\r\n
*/
void process_rtk_data(uint8_t *buf, uint16_t len)
{
    char *save_ptr  = NULL;
    char *line = strtok_r((char*)buf, "\r\n", &save_ptr);
    if (line == NULL) return;

    // 加上 "\r\n"
    uint16_t line_len = strlen(line);
    char send_buf[256];
    memcpy(send_buf, line, line_len);
    send_buf[line_len] = '\r';
    send_buf[line_len + 1] = '\n';
    uint16_t send_len = line_len + 2; // 总长度=行长度+换行符

    //4g mqtt
    HAL_UART_Transmit(&huart4, (uint8_t*)send_buf, send_len, 100);
}


void process_pps_data(uint8_t *buf, uint16_t len)
{
    char *save_ptr  = NULL;
    char *line = strtok_r((char*)buf, "\r\n", &save_ptr);
    if (line == NULL) return;

    // 加上 "\r\n"
    uint16_t line_len = strlen(line);
    char send_buf[256];
    memcpy(send_buf, line, line_len);
    send_buf[line_len] = '\r';
    send_buf[line_len + 1] = '\n';
    uint16_t send_len = line_len + 2; // 总长度=行长度+换行符

    //4g mqtt
    HAL_UART_Transmit(&huart4, (uint8_t*)send_buf, send_len, 100);
}


void process_acoustic_data(uint8_t *buf, uint16_t len)
{
    //4g mqtt
    HAL_UART_Transmit(&huart4, buf, len, 100);
}

//init
void app_init(void)
{
    //RTK 上电大约4s 后才有输出，4G 模块联网也需要时间

    //4G PWR 使能
    HAL_GPIO_WritePin(ML307_PWR_GPIO_Port, ML307_PWR_Pin, GPIO_PIN_SET);
    HAL_Delay(500);

    //空闲中断DMA 接收不定长数据
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_buf, UART1_RX_BUFF_SIZE); // 信标测距
		HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_buf, UART2_RX_BUFF_SIZE); // 高精度时钟
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart3_rx_buf, UART3_RX_BUFF_SIZE); // RTK信息
}

//loop
void app_task(void)
{
	
    if (uart1_rx_flag == 1) {
        uart1_rx_flag = 0;

        if (memcmp(uart1_rx_buf, "$HHRNG", 6) == 0) {
            got_hhrng = 1;

            process_acoustic_data(uart1_rx_buf, uart1_rx_len);
        }
    }

    if (uart2_rx_flag == 1) {
        uart2_rx_flag = 0;
				if (uart2_rx_len == 0) return;

				// 复制并补 '\0'，避免 strtok_r 越界
				char tmp[UART2_RX_BUFF_SIZE + 1];
				uint16_t copy_len = uart2_rx_len;
				if (copy_len > UART2_RX_BUFF_SIZE) copy_len = UART2_RX_BUFF_SIZE; // 保险
				memcpy(tmp, uart2_rx_buf, copy_len);
				tmp[copy_len] = '\0';

				// 按 CR/LF 切行
				char *save_ptr = NULL;
				for (char *line = strtok_r(tmp, "\r\n", &save_ptr);
						 line != NULL;
						 line = strtok_r(NULL, "\r\n", &save_ptr))
				{
						if (strncmp(line, "$GNTXT", 6) == 0) {
								// 传给你给的发送函数。该函数会自动在末尾加 "\r\n"
								process_pps_data((uint8_t*)line, (uint16_t)strlen(line));
								return; // 找到第一条就发并返回
						}
				}
//        if (memcmp(uart2_rx_buf, "$GNTXT", 6) == 0) {
//            got_txt = 1;

//            process_pps_data(uart2_rx_buf, uart2_rx_len);
//        }
    }
		
    if (uart3_rx_flag == 1) {
        uart3_rx_flag = 0;
        
        if (memcmp(uart3_rx_buf, "$GNGGA", 6) == 0 ) {
            got_gga = 1;

            process_rtk_data(uart3_rx_buf, uart3_rx_len);
        }

        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }



    //10s 内收到两种数据喂狗
//    if (got_gga && got_hhrng)
    {
        got_gga = 0;
        got_hhrng = 0;

        feed_watchdog();
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    /* Prevent unused argument(s) compilation warning */
    if(huart == &huart1) {
        uart1_rx_flag = 1;
        uart1_rx_len = Size;
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_buf, UART1_RX_BUFF_SIZE);
    }
    else if(huart == &huart2) {
        uart2_rx_flag = 1;
        uart2_rx_len = Size;
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_buf, UART2_RX_BUFF_SIZE);
    }
    else if(huart == &huart3) {
        uart3_rx_flag = 1;
        uart3_rx_len = Size;
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart3_rx_buf, UART3_RX_BUFF_SIZE);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        //清除所有错误标志（防止因噪声、溢出等引发问题）
        __HAL_UART_CLEAR_PEFLAG(&huart1);
        __HAL_UART_CLEAR_FEFLAG(&huart1);
        __HAL_UART_CLEAR_NEFLAG(&huart1);
        __HAL_UART_CLEAR_OREFLAG(&huart1);

        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_buf, UART1_RX_BUFF_SIZE);
    }
    else if (huart == &huart2)
    {
        //清除所有错误标志（防止因噪声、溢出等引发问题）
        __HAL_UART_CLEAR_PEFLAG(&huart2);
        __HAL_UART_CLEAR_FEFLAG(&huart2);
        __HAL_UART_CLEAR_NEFLAG(&huart2);
        __HAL_UART_CLEAR_OREFLAG(&huart2);

        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_buf, UART2_RX_BUFF_SIZE);
    }
    else if (huart == &huart3)
    {
        //清除所有错误标志（防止因噪声、溢出等引发问题）
        __HAL_UART_CLEAR_PEFLAG(&huart3);
        __HAL_UART_CLEAR_FEFLAG(&huart3);
        __HAL_UART_CLEAR_NEFLAG(&huart3);
        __HAL_UART_CLEAR_OREFLAG(&huart3);

        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart3_rx_buf, UART3_RX_BUFF_SIZE);
    }
}
