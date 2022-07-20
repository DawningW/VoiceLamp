/**
 * @file main.c
 * @brief 语音小夜灯
 * @version 1.0
 * @date 2022-6-26
 */
#include <stdio.h> 
#include "FreeRTOS.h"
#include "task.h"

#include "ci112x_uart.h"
#include "ci112x_scu.h"
#include "ci112x_pwm.h"
#include "ci112x_gpio.h"
#include "ci112x_TimerWdt.h"
#include "ci112x_timer.h"
#include "ci112x_iwdg.h"
#include "ci112x_spiflash.h"
#include "ci112x_system.h"
#include "ci112x_core_eclic.h"
#include "ci112x_core_misc.h"
#include "ci112x_codec.h"

#include "ci_debug_config.h"
#include "ci_log.h"
#include "platform_config.h"
#include "sdk_default_config.h"
#include "audio_play_api.h"
#include "audio_play_decoder.h"
#include "audio_in_manage.h"
#include "system_msg_deal.h"
#include "command_info.h"
#include "prompt_player.h"
#include "asr_api.h"
#include "get_play_data.h"
#include "flash_rw_process.h"
#include "ci_flash_data_info.h"
#include "ci_nvdata_manage.h"
#include "ci_system_info.h"
#include "ci_debug_config.h"
#include "ci_fft.h"
#include "ci_task_monitor.h"
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_RTT.h"

/* 音频输入任务句柄及系统监控id */
uint8_t audio_in_preprocess_mode_id;
TaskHandle_t audio_in_preprocess_mode_handle;

/**
 * @brief 系统异常复位前
 */
void sys_reset_callback(void) {}

/**
 * @brief 硬件初始化和平台初始化相关代码
 *          这个函数主要用于系统上电后初始化硬件寄存器到初始值，配置中断向量表初始化芯片io配置时钟
 *          配置完成后，系统时钟配置完毕，相关获取clk的函数可以正常调用
 *
 * @note 在这里初始化硬件需要注意：
 *          由于部分驱动代码中使用os相关接口，在os运行前调用这些接口会导致中断被屏蔽
 *          其中涉及的驱动包括：QSPIFLASH、DMA、I2C、SPI
 *          所以这些外设的初始化需要放置在vTaskVariablesInit进行。
 *          如一定需要（非常不建议）在os运行前初始化这些驱动，请仔细确认保证：
 *              1.CONFIG_DIRVER_BUF_USED_FREEHEAP_EN 宏配置为0
 *              2.DRIVER_OS_API                      宏配置为0
 */
static void hardware_init(void)
{
    /* 配置外设复位，硬件外设初始化 */
    extern void SystemInit(void);
    SystemInit();
    /* 设置中断优先级分组 */
    eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO0);
    /* 开启全局中断 */
    eclic_global_interrupt_enable();
    /* 初始化系统 */
    enable_mcycle_minstret();
    init_platform();
    /* debug port */
#if ((HAL_UART0_BASE) == (CONFIG_CI_LOG_UART))
    UARTPollingConfig(UART0);
#elif ((HAL_UART1_BASE) == (CONFIG_CI_LOG_UART))
    UARTPollingConfig(UART1);
#endif
    ci_loginfo(LOG_USER, "Debug port init success!\n");
#if (CONFIG_CI_LOG_UART == UART_PROTOCOL_NUMBER && MSG_COM_USE_UART_EN)
    CI_ASSERT(0, "Log uart and protocol uart confict!\n");
#endif
#if CONFIG_SYSTEMVIEW_EN
    /* 初始化SysView RTT，仅用于调试 */
    SEGGER_SYSVIEW_Conf();
    /* 使用串口方式输出SysView信息 */
    vSYSVIEWUARTInit();
    ci_logdebug(CI_LOG_DEBUG, "Segger Sysview Control Block Detection Address is 0x%x\n",&_SEGGER_RTT);
#endif
}

/**
 * @brief 任务间通信变量的初始化，例如消息队列、信号量等的创建、驱动包含OS相关外设初始化
 */
static void vTaskVariablesInit(void)
{
#if DRIVER_OS_API
    /* dma中断事件标志组初始化 */
    dma_int_event_group_init();
    /* iic中断事件标志组初始化 */
    iic_int_event_group_init();
#endif
    /* SPI Flash初始化并判断接口模式 */
    if (RETURN_OK != flash_init(QSPI0))
    {
        CI_ASSERT(0, "Flash init failed!\n");
    }
    ci_loginfo(LOG_USER, "Flash current mode is %s!!!\n",
            flash_check_mode(QSPI0) == 1 ? "quad" : "stand");
#if AUDIO_PLAYER_ENABLE
    /* 注册prompt解码器 */
#if USE_PROMPT_DECODER
    registe_decoder_ops(&prompt_decoder);  
#endif
    /* 注册mp3解码器 */
#if USE_MP3_DECODER
    registe_decoder_ops(&mp3_decoder);    
#endif
    /* 注册aac解码器 */
#if USE_AAC_DECODER
    registe_decoder_ops(&aac_decoder);    
#endif
    /* 注册ms_wav解码器 */
#if USE_MS_WAV_DECODER
    registe_decoder_ops(&ms_wav_decoder); 
#endif
    /* 注册flac解码器 */
#if USE_FLAC_DECODER
    registe_decoder_ops(&flac_decoder); 
#endif
#endif
    /* 系统消息任务资源初始化 */
    sys_msg_task_initial();
    /* flash控制信号初始化，这个模块用于保证系统访问flash和dnn硬件访问flash不发生冲突 */
    flash_ctl_init();
    /* flash固件信息解析并初始化固件信息结构，DEFAULT_MODEL_GROUP_ID为默认模型分组ID，开机后第一次运行的识别环境 */
    ci_flash_data_info_init(DEFAULT_MODEL_GROUP_ID);
    /* 配置CODEC */
    Scu_SetDeviceGate(HAL_CODEC_BASE, 1);
    Scu_Setdevice_Reset(HAL_CODEC_BASE);
    Scu_Setdevice_ResetRelease(HAL_CODEC_BASE);
    inner_codec_reset();
    inner_codec_power_up(INNER_CODEC_CURRENT_128I);
#if AUDIO_PLAYER_ENABLE
    inner_codec_dac_first_enable();
    pa_switch_io_init();
#endif
    /* 初始化系统监控任务所需资源，sys_reset_callback为注册的系统死机复位前的hook函数 */
    monitor_creat(sys_reset_callback);
}

/**
 * @brief 启动任务
 * @note 用来创建所需资源以及其他各个组件的任务
 * @param p_arg 任务参数
 */
static void vTaskCreate(void *p_arg)
{
    /* 创建任务所需信号量队列 */
    vTaskVariablesInit();
    /* 设置任务启动flag */
    set_asr_run_flag();
#if AUDIO_PLAYER_ENABLE
    /* 播放器任务 */
    audio_play_init();
#endif
    /* 用户任务 */
    xTaskCreate(UserTaskManageProcess, "user_task", 320, NULL, 4, NULL);
    /* ASR系统启动任务 */
    xTaskCreate(asr_system_startup_task, "asr_system_startup", 512, NULL, 4, NULL);
    /* 语音输入任务 */
    xTaskCreate(audio_in_preprocess_mode_task, "audio_in_preprocess", 512,
            &audio_in_preprocess_mode_id, 4, &audio_in_preprocess_mode_handle);
    join_monitor(&audio_in_preprocess_mode_id, 50, audio_in_preprocess_mode_handle);
    /* 命令行任务 */
#if CONFIG_CLI_EN
    /* 注册示例命令 */
    vRegisterCLICommands();
    /* 启动CLI */
    vUARTCommandConsoleStart(768, 1);
#endif
    /* 系统监控任务 */
    xTaskCreate(task_monitor, "task_monitor", 160, NULL, 5, NULL);
    /*
    for (;;)
    {
        get_task_status();
        get_fmem_status();
        get_mem_status();
        ci_logdebug(LOG_SYS_INFO, "Current tick is %ld\n", xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(5000));
        //_delay(10000);
    }
    */
    vTaskDelete(NULL);
}

/**
 * @brief 主函数，进入应用程序的入口
 */
int main(void)
{
    /* 硬件平台初始化 */
    hardware_init();
    /* 版本信息 */
    ci_loginfo(LOG_USER, "\r\n\r\n");
    ci_loginfo(LOG_USER, "ci112x_sdk_%s_%d.%d.%d Built-in\r\n",
                SDK_TYPE, SDK_VERSION, SDK_SUBVERSION, SDK_REVISION);
    ci_loginfo(LOG_USER, "\033[1;32mWelcome to use voice lamp V%d.%d, made by WC.\033[0;39m\r\n",
            USER_VERSION_MAIN_NO, USER_VERSION_SUB_NO);

    /* 创建启动任务 */
    xTaskCreate(vTaskCreate, "task_create", 480, NULL, 4, NULL);
    /* 启动调度，开始执行任务 */
    vTaskStartScheduler();
    /* 进入死循环 */
    while (1);
}
