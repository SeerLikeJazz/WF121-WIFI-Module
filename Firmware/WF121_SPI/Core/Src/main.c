/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "retarget.h"
#include "wifi_bglib.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define cs_high  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
#define cs_low  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t ReceivedFlag = 0;
uint8_t SendBuf[220];
uint32_t m_len_sent;

BGLIB_DEFINE()

//Notification counters
//interrupt will increment notifications
//main loop reads messages and increments notifications_handled
volatile uint32_t notifications=0;
uint32_t notifications_handled=0;
//receive buffer
uint8_t rx_buf[4096];
//temporary buffer to receive data when sending commands
uint8_t tmp_buf[256];
uint8_t tmp_idx_w=0;
uint8_t tmp_idx_r=0;

// Set IP, Netmask, Gateway and SSID
ipv4 ip_address = {.a[0] = 192, .a[1] = 168, .a[2] = 1, .a[3] = 1};
ipv4 netmask = {.a[0] = 255, .a[1] = 255, .a[2] = 255, .a[3] = 0};
ipv4 gateway = {.a[0] = 192, .a[1] = 168, .a[2] = 1, .a[3] = 1};
uint8_t SSID[12] = {'i','R','e','c','o','r','d','e','r','W','3','2'};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void spi_output(uint8 len1,uint8* data1,uint16 len2,uint8* data2)
{//when outputting spi message we have to buffer incoming bytes and check their validity later
    if(len1) {
        cs_low;
        HAL_SPI_TransmitReceive(&hspi1, data1, &tmp_buf[0], len1,0xFF);
//        cs_high;

    }
    if(len2) {
//        cs_low;
        HAL_SPI_TransmitReceive(&hspi1, data2, &tmp_buf[0], len2,0xFF);
        cs_high;
    }
}

char spi_read()
{//read buffered messages first
    uint8_t TxData = 0;
    uint8_t Rxdata;

    cs_low;
    HAL_SPI_TransmitReceive(&hspi1, &TxData, &Rxdata, 1,0xFF);
//    cs_high;
    return Rxdata;// get the received data
}

int read_header(uint8_t *rx)
{
    int i;
    *rx=spi_read();
    if(*rx==0)//first byte is 0, not valid message. SPI interface uses 0 as padding byte
        return -1;
    rx++;
    for(i=0;i<BGLIB_MSG_HEADER_LEN-1;i++)
    {
        *rx++=spi_read();
    }

    return 0;
}
void read_message()
{
    int n;
    struct wifi_cmd_packet *pck;
    uint8_t *rx=&rx_buf[BGLIB_MSG_HEADER_LEN];
    if(read_header(rx_buf))
        return ;//no message, return
    for(n=0;n<BGLIB_MSG_LEN(rx_buf);n++)
    {//read payload
        *rx++=spi_read();
    }

    //find message
    pck=BGLIB_MSG(rx_buf);
    switch(BGLIB_MSG_ID(rx_buf))
    {
        case wifi_evt_system_boot_id:
            //module has booted, synchronize handled notifications to received interrupts
            notifications_handled=notifications;
            printf("wifi_evt_system_boot\n");
            printf("Sending command: wifi_cmd_sme_set_operating_mode(2)\n");
            wifi_cmd_sme_set_operating_mode(2);
            return ;//return as no need to increment notifications handled
        case wifi_rsp_sme_set_operating_mode_id:
            printf("Command response received: wifi_rsp_sme_set_operating_mode_id\n");
            printf("Sending command: wifi_cmd_sme_wifi_on\n");
            wifi_cmd_sme_wifi_on();
            break;
        case wifi_rsp_sme_wifi_on_id:
            printf("Command response received: wifi_rsp_sme_wifi_on_id\n");
            break;
        case wifi_evt_sme_wifi_is_on_id:
            printf("Event received: wifi_evt_sme_wifi_is_on\n");
            printf("Sending command: wifi_cmd_sme_start_ap_mode\n");
            wifi_cmd_sme_start_ap_mode(11,0,12,SSID);
            break;
        case wifi_rsp_sme_start_ap_mode_id:
            printf("Command response received: wifi_rsp_sme_start_ap_mode_id\n");
            break;
        case wifi_evt_sme_ap_mode_started_id:
            printf("Event received: wifi_evt_sme_ap_mode_started_id\n");
//            ap_mode_started = true;
//            printf("Sending command: wifi_cmd_https_enable\n");
//            wifi_cmd_https_enable(0, 1, 0);
            break;
        case wifi_evt_tcpip_configuration_id:
            /* This event is triggered by other commands such as start_ap_mode command.
             * The ap_mode_started bool prevents it from being issued more than once. */
            printf("Event received: wifi_evt_tcpip_configuration_id\n");
//            if(!ap_mode_started) {
//                printf("Sending command: wifi_cmd_sme_start_ap_mode\n");
//                wifi_cmd_sme_start_ap_mode(11,0,11,SSID);
//            }
            break;
        case wifi_evt_tcpip_dns_configuration_id:
            printf("Event received: wifi_evt_tcpip_dns_configuration_id\n");
            break;
        case wifi_evt_sme_interface_status_id:
            printf("Event received: wifi_evt_sme_interface_status_id\n");
            printf("Sending command: wifi_cmd_https_enable\n");
            wifi_cmd_https_enable(0, 1, 0);
            break;
        case wifi_rsp_https_enable_id:
            printf("Command response received: wifi_rsp_https_enable_id\n");
            printf("Sending command: wifi_cmd_tcpip_start_tcp_server\n");
            wifi_cmd_tcpip_start_tcp_server(5001,0);//Understanding Endpoints:0 for SPI3
            break;
        case wifi_rsp_tcpip_start_tcp_server_id:
            printf("Command response received: wifi_rsp_tcpip_start_tcp_server_id, result:%d, endpoint:%d\n",pck->rsp_tcpip_start_tcp_server.result,pck->rsp_tcpip_start_tcp_server.endpoint);
            break;
        case wifi_evt_tcpip_endpoint_status_id:
            printf("Event received: wifi_evt_tcpip_endpoint_status_id, endpoint:%d, local_ip:%d\n",pck->evt_tcpip_endpoint_status.endpoint,pck->evt_tcpip_endpoint_status.local_port);
            break;
        case wifi_evt_endpoint_status_id:
            printf("Event received: wifi_evt_endpoint_status_id, endpoint:%d, type:%d, streaming:%d,destination:%d, active:%d\n",
                   pck->evt_endpoint_status.endpoint,
                   pck->evt_endpoint_status.type,
                   pck->evt_endpoint_status.streaming,
                   pck->evt_endpoint_status.destination,
                   pck->evt_endpoint_status.active);
//            if(pck->evt_endpoint_status.active == 0){
//                printf("receiving and sending of data is blocked\n");
//            }
//            else{
//                printf("receiving and sending is allowed\n");
//            }
            break;


        //client连接上
        case wifi_evt_sme_ap_client_joined_id:
            printf("Client joined with MAC %02X %02X %02X %02X %02X %02X\n",
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[0],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[1],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[2],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[3],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[4],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[5]
            );
            break;
        //client断开
        case wifi_evt_sme_ap_client_left_id:
            printf("Client left with MAC %02X %02X %02X %02X %02X %02X\n",
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[0],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[1],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[2],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[3],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[4],
                   BGLIB_MSG(rx_buf)->evt_sme_ap_client_joined.mac_address.addr[5]
            );
            break;

        //接收到client数据
        case wifi_evt_endpoint_data_id:
            for(int n=0; n<BGLIB_MSG(rx_buf)->evt_endpoint_data.data.len; n++) {
                printf("%c",(char)BGLIB_MSG(rx_buf)->evt_endpoint_data.data.data[n]);
            }
            if(ReceivedFlag == 0) {
                ReceivedFlag = 1;
                for (int i = 0; i < 220; ++i) {
                    SendBuf[i] = i;
                }
                wifi_cmd_endpoint_send(pck->evt_endpoint_data.endpoint,220,SendBuf);
            }

            break;
        //WF121发送完成response
        case wifi_rsp_endpoint_send_id:
//            printf("Command response received: wifi_rsp_endpoint_send_id\n");
            if(pck->rsp_endpoint_send.result == 0){//发送成功
                m_len_sent++;
                wifi_cmd_endpoint_send(pck->rsp_endpoint_send.endpoint,220,SendBuf);
            }
            else if(pck->rsp_endpoint_send.result == wifi_err_buffers_full) {
                /* WF121 buffers are full -> wait for endpoint status that indicates there's free space */
//                printf("WF121 buffers are full\n");
                wifi_cmd_endpoint_send(pck->rsp_endpoint_send.endpoint,220,SendBuf);
            }
            else{
                printf("err %d\n", pck->rsp_endpoint_send.result);
            }

            break;



        default:
            // Some events are not important so they don't need any actions
            printf("Unparsed Event\n");
            break;
    }


    notifications_handled++;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
    BGLIB_INITIALIZE(spi_output);
    RetargetInit(&huart1);
    printf("Hello Word - This is WF121 SPI Demo\n");
//    wifi_cmd_system_reset(0);
    HAL_Delay(500);
    HAL_GPIO_WritePin(MCLR_GPIO_Port, MCLR_Pin, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      if((notifications>notifications_handled))
      {//if WF121 notifies of new messages, or we sent data and need to check for possible incoming messages
          read_message();
      }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == NOTIFY_Pin)
    {
        notifications++;
//        printf("AA\n");
    }
}

uint16_t time_ms = 0;
void HAL_SYSTICK_Callback(void)
{
    time_ms++;
    if(time_ms == 1000*5) {
        time_ms = 0;
        printf("==**Speed: %d KB/s**==\n",m_len_sent*220/1000/5);
        m_len_sent = 0;
    }

}

/** 先将时钟源选择为内部时钟*/
//RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
//RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
//if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
//{
//  Error_Handler();
//}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
