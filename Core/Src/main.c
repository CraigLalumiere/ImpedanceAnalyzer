/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "analyzer.h"
#include "blinky.h"
#include "bsp.h"
// #include "cli.h"
#include "pc_com.h"
#include "posted_signals.h"
#include "qpc.h"
#include "reset.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// QP Priorities for Active Objects must be unique
// Lower number is lower priority
// 0 is reserved, lowest available is 1
typedef enum
{
    AO_RESERVED = 0U,
    AO_PRIO_BLINKY,
    AO_PRIO_APP_CLI,
    AO_PRIO_APP_PC_COM,
    AO_PRIO_ANALYZER,
} AO_Priority_T;

typedef struct
{
    union
    {
        QEvt base_event;
        FloatEvent_T float_event;
    } small_messages;
} SmallMessageUnion_T;
typedef struct
{
    union
    {
        QEvt someMultipleQEvt[4];
        DebugForceFaultEvent_T fault_event;
        DrawBodePlotEvent_T plot_bode_plot_event;
    } medium_messages;
} MediumMessageUnion_T;
typedef struct
{
    union
    {
        QEvt base_event;
        PCCOMPrintEvent_T pc_com_print_evt;
        PCCOMCliDataEvent_T pc_com_cli_event;
    } large_messages;
} LongMessageUnion_T;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

UART_HandleTypeDef hlpuart1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim8;

extern bool sinusoid_running;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

inline void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (sinusoid_running)
    {
        sinusoid_running        = false;
        static QEvt const event = QEVT_INITIALIZER(POSTED_WAVEFORM_CAPTURE_COMPLETE_SIG);
        QACTIVE_POST(AO_Analyzer, &event, NULL);
    }
}
inline void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
}

inline void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
}

inline void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == htim1.Instance)
    {
        HAL_GPIO_TogglePin(DEBUG_GPIO_GPIO_Port, DEBUG_GPIO_Pin);
        // DMA_timer_stop();
    }
}

inline void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    // if (htim->Instance == htim1.Instance)
    // {
    //     static QEvt const event = QEVT_INITIALIZER(POSTED_WAVEFORM_CAPTURE_COMPLETE_SIG);
    //     QACTIVE_POST(AO_Analyzer, &event, NULL);
    // }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    Reset_Init();
    QF_init(); // initialize the framework and the underlying RT kernel
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    // IN CUBEMX, BE SURE TO SET INTERRUPT PRIORITY FOR ALL "QP AWARE" INTERRUPTS TO AT LEAST 4
    // SINCE QF_AWARE_ISR_CMSIS_PRI is 3

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_DAC1_Init();
    MX_LPUART1_UART_Init();
    MX_ADC1_Init();
    MX_TIM8_Init();
    MX_TIM1_Init();
    MX_ADC2_Init();
    /* USER CODE BEGIN 2 */

    uint16_t priority = QF_AWARE_ISR_CMSIS_PRI;
    (void) priority;

    // ADC setup
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

    // __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_CC2);
    // TIM1->CNT = 0x0; // begin timer
    // startup PWM for tim1 and tim8 (just for debugging)
    if (HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    BSP_Init();

    // initialize event pools
    static QF_MPOOL_EL(SmallMessageUnion_T) smlPoolSto[10];
    QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));

    static QF_MPOOL_EL(MediumMessageUnion_T) mediumPoolSto[20];
    QF_poolInit(mediumPoolSto, sizeof(mediumPoolSto), sizeof(mediumPoolSto[0]));

    static QF_MPOOL_EL(LongMessageUnion_T) longPoolSto[20];
    QF_poolInit(longPoolSto, sizeof(longPoolSto), sizeof(longPoolSto[0]));

    size_t smallsize  = sizeof(SmallMessageUnion_T);
    size_t mediumsize = sizeof(MediumMessageUnion_T);
    size_t largesize  = sizeof(LongMessageUnion_T);
    size_t mysize     = sizeof(DrawPlotEvent_T);
    (void) smallsize;
    (void) mediumsize;
    (void) largesize;
    (void) mysize;

    // initialize publish-subscribe
    static QSubscrList subscrSto[PUBSUB_MAX_SIG];
    QActive_psInit(subscrSto, Q_DIM(subscrSto));

    // instantiate and start AOs/threads...

    static QEvt const *blinkyQueueSto[10];
    Blinky_ctor();
    QACTIVE_START(
        AO_Blinky,
        AO_PRIO_BLINKY,        // QP prio. of the AO
        blinkyQueueSto,        // event queue storage
        Q_DIM(blinkyQueueSto), // queue length [events]
        (void *) 0,
        0U,          // no stack storage
        (void *) 0); // no initialization param

    // static QEvt const *cli_QueueSto[20];
    // AppCLI_ctor(BSP_Get_Serial_IO_Interface_UART());
    // QACTIVE_START(
    //     AO_AppCLI,
    //     AO_PRIO_APP_CLI,     // QP prio. of the AO
    //     cli_QueueSto,        // event queue storage
    //     Q_DIM(cli_QueueSto), // queue length [events]
    //     (void *) 0,          // stack storage (not used in QK)
    //     0U,                  // stack size [bytes] (not used in QK)
    //     (void *) 0);         // no initialization param

    static QEvt const *app_pc_com_QueueSto[600];
    PC_COM_ctor(BSP_Get_Serial_IO_Interface_UART());
    QACTIVE_START(
        AO_PC_COM,
        AO_PRIO_APP_PC_COM,         // QP prio. of the AO
        app_pc_com_QueueSto,        // event queue storage
        Q_DIM(app_pc_com_QueueSto), // queue length [events]
        (void *) 0,                 // stack storage (not used in QK)
        0U,                         // stack size [bytes] (not used in QK)
        (void *) 0);                // no initialization param

    static QEvt const *analyzer_QueueSto[10];
    Analyzer_ctor();
    QACTIVE_START(
        AO_Analyzer,
        AO_PRIO_ANALYZER,         // QP prio. of the AO
        analyzer_QueueSto,        // event queue storage
        Q_DIM(analyzer_QueueSto), // queue length [events]
        (void *) 0,               // stack storage (not used in QK)
        0U,                       // stack size [bytes] (not used in QK)
        (void *) 0);              // no initialization param

    return QF_run(); // run the QF application
                     /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    // while (1)
    // {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

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
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV2;
    RCC_OscInitStruct.PLL.PLLN            = 42;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV6;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{
    /* USER CODE BEGIN ADC1_Init 0 */

    /* USER CODE END ADC1_Init 0 */

    ADC_MultiModeTypeDef multimode = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */

    /** Common config
     */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution            = ADC_RESOLUTION_8B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation      = 0;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIG_T8_TRGO2;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure the ADC multi-mode
     */
    multimode.Mode             = ADC_DUALMODE_REGSIMULT;
    multimode.DMAAccessMode    = ADC_DMAACCESSMODE_DISABLED;
    multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel      = ADC_CHANNEL_1;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN ADC1_Init 2 */

    /* USER CODE END ADC1_Init 2 */
}

/**
 * @brief ADC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC2_Init(void)
{
    /* USER CODE BEGIN ADC2_Init 0 */

    /* USER CODE END ADC2_Init 0 */

    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC2_Init 1 */

    /* USER CODE END ADC2_Init 1 */

    /** Common config
     */
    hadc2.Instance                   = ADC2;
    hadc2.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
    hadc2.Init.Resolution            = ADC_RESOLUTION_8B;
    hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc2.Init.GainCompensation      = 0;
    hadc2.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc2.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc2.Init.LowPowerAutoWait      = DISABLE;
    hadc2.Init.ContinuousConvMode    = DISABLE;
    hadc2.Init.NbrOfConversion       = 1;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.DMAContinuousRequests = ENABLE;
    hadc2.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc2.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel      = ADC_CHANNEL_2;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN ADC2_Init 2 */

    /* USER CODE END ADC2_Init 2 */
}

/**
 * @brief DAC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_DAC1_Init(void)
{
    /* USER CODE BEGIN DAC1_Init 0 */

    /* USER CODE END DAC1_Init 0 */

    DAC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN DAC1_Init 1 */

    /* USER CODE END DAC1_Init 1 */

    /** DAC Initialization
     */
    hdac1.Instance = DAC1;
    if (HAL_DAC_Init(&hdac1) != HAL_OK)
    {
        Error_Handler();
    }

    /** DAC channel OUT1 config
     */
    sConfig.DAC_HighFrequency           = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
    sConfig.DAC_DMADoubleDataMode       = DISABLE;
    sConfig.DAC_SignedFormat            = DISABLE;
    sConfig.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
    sConfig.DAC_Trigger                 = DAC_TRIGGER_T8_TRGO;
    sConfig.DAC_Trigger2                = DAC_TRIGGER_NONE;
    sConfig.DAC_OutputBuffer            = DAC_OUTPUTBUFFER_ENABLE;
    sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    sConfig.DAC_UserTrimming            = DAC_TRIMMING_FACTORY;
    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN DAC1_Init 2 */

    /* USER CODE END DAC1_Init 2 */
}

/**
 * @brief LPUART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_LPUART1_UART_Init(void)
{
    /* USER CODE BEGIN LPUART1_Init 0 */

    /* USER CODE END LPUART1_Init 0 */

    /* USER CODE BEGIN LPUART1_Init 1 */

    /* USER CODE END LPUART1_Init 1 */
    hlpuart1.Instance                    = LPUART1;
    hlpuart1.Init.BaudRate               = 500000;
    hlpuart1.Init.WordLength             = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits               = UART_STOPBITS_1;
    hlpuart1.Init.Parity                 = UART_PARITY_NONE;
    hlpuart1.Init.Mode                   = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&hlpuart1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN LPUART1_Init 2 */

    /* USER CODE END LPUART1_Init 2 */
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{
    /* USER CODE BEGIN TIM1_Init 0 */

    /* USER CODE END TIM1_Init 0 */

    TIM_SlaveConfigTypeDef sSlaveConfig                 = {0};
    TIM_MasterConfigTypeDef sMasterConfig               = {0};
    TIM_OC_InitTypeDef sConfigOC                        = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    /* USER CODE BEGIN TIM1_Init 1 */

    /* USER CODE END TIM1_Init 1 */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 65535;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_OC_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    sSlaveConfig.SlaveMode    = TIM_SLAVEMODE_EXTERNAL1;
    sSlaveConfig.InputTrigger = TIM_TS_ITR5;
    if (HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger  = TIM_TRGO_OC2REF;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode       = TIM_OCMODE_RETRIGERRABLE_OPM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime         = 0;
    sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter      = 0;
    sBreakDeadTimeConfig.BreakAFMode      = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.Break2State      = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity   = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter     = 0;
    sBreakDeadTimeConfig.Break2AFMode     = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM1_Init 2 */

    /* USER CODE END TIM1_Init 2 */
    HAL_TIM_MspPostInit(&htim1);
}

/**
 * @brief TIM8 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM8_Init(void)
{
    /* USER CODE BEGIN TIM8_Init 0 */

    /* USER CODE END TIM8_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig           = {0};
    TIM_SlaveConfigTypeDef sSlaveConfig                 = {0};
    TIM_MasterConfigTypeDef sMasterConfig               = {0};
    TIM_OC_InitTypeDef sConfigOC                        = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    /* USER CODE BEGIN TIM8_Init 1 */

    /* USER CODE END TIM8_Init 1 */
    htim8.Instance               = TIM8;
    htim8.Init.Prescaler         = 0;
    htim8.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim8.Init.Period            = 13; // 13
    htim8.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim8.Init.RepetitionCounter = 5 - 1;
    htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Init(&htim8) != HAL_OK)
    {
        Error_Handler();
    }
    sSlaveConfig.SlaveMode    = TIM_SLAVEMODE_GATED;
    sSlaveConfig.InputTrigger = TIM_TS_ITR0;
    if (HAL_TIM_SlaveConfigSynchro(&htim8, &sSlaveConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger  = TIM_TRGO_OC2REF;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_UPDATE;
    sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 5;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime         = 0;
    sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter      = 0;
    sBreakDeadTimeConfig.BreakAFMode      = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.Break2State      = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity   = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter     = 0;
    sBreakDeadTimeConfig.Break2AFMode     = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM8_Init 2 */

    /* USER CODE END TIM8_Init 2 */
    HAL_TIM_MspPostInit(&htim8);
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{
    /* DMA controller clock enable */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    /* DMA1_Channel2_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, GS1_Pin | GS2_Pin | GS0_Pin | DEBUG_GPIO_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : FW_LED_Pin */
    GPIO_InitStruct.Pin   = FW_LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FW_LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : GS1_Pin GS2_Pin GS0_Pin DEBUG_GPIO_Pin */
    GPIO_InitStruct.Pin   = GS1_Pin | GS2_Pin | GS0_Pin | DEBUG_GPIO_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef USE_FULL_ASSERT
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
