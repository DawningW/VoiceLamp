#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "system_msg_deal.h"
#include "prompt_player.h"
#include "voice_module_uart_protocol.h"
#include "i2c_protocol_module.h"
#include "ci_nvdata_manage.h"
#include "ci_log.h"
#include "ci112x_gpio.h"
#include "light.h"

///tag-insert-code-pos-1

/**
 * @brief 用户初始化
 */
void userapp_initial(void)
{
#if CPU_RATE_PRINT
    init_timer3_getresource();
#endif
#if MSG_COM_USE_UART_EN
#if (UART_PROTOCOL_VER == 1)
    uart_communicate_init();
#elif (UART_PROTOCOL_VER == 2)
    vmup_communicate_init();
#elif (UART_PROTOCOL_VER == 255)
    UARTInterruptConfig((UART_TypeDef*) UART_PROTOCOL_NUMBER, UART_PROTOCOL_BAUDRATE);
#endif
#endif
#if MSG_USE_I2C_EN
    i2c_communicate_init();
#endif
#if IIS1_ENABLE == 0
    //IIS1引脚配置为gpio
    Scu_SetDeviceGate(HAL_GPIO2_BASE, ENABLE);
    Scu_SetIOReuse(I2S1_LRCLK_PAD, FIRST_FUNCTION);
    Scu_SetIOReuse(I2S1_SDO_PAD, FIRST_FUNCTION);
    Scu_SetIOReuse(I2S1_SCLK_PAD, FIRST_FUNCTION);
#endif
    ///tag-gpio-init
    if (light_init() != RETURN_OK)
    {
        ci_logerr(LOG_USER, "init light failed!\n");
    }
}

/**
 * @brief 处理按键消息（目前未实现该demo）
 *
 * @param key_msg 按键消息
 */
void userapp_deal_key_msg(sys_msg_key_data_t *key_msg)
{
    (void) key_msg;
}

/**
 * @brief 按语义ID响应asr消息处理
 *
 * @param asr_msg
 * @param cmd_handle
 * @param semantic_id
 * @return uint32_t
 */
uint32_t deal_asr_msg_by_semantic_id(sys_msg_asr_data_t *asr_msg, cmd_handle_t cmd_handle, uint32_t semantic_id)
{
    uint32_t ret = 0;
    int select_index = -1;
    if (get_product_id_from_semantic_id(semantic_id) == PRODUCT_LAMP_CONTROL)
    {
        ret = 1;
        switch (get_function_id_from_semantic_id(semantic_id))
        {
            case VOLUME_UP: //增大音量
                vol_set(vol_get() + 1);
                select_index = (vol_get() == VOLUME_MAX) ? 1 : 0;
                break;
            case VOLUME_DOWN: //减小音量
                vol_set(vol_get() - 1);
                select_index = (vol_get() == VOLUME_MIN) ? 1 : 0;
                break;
            case MAXIMUM_VOLUME: //最大音量
                vol_set(VOLUME_MAX);
                break;
            case MEDIUM_VOLUME: //中等音量
                vol_set(VOLUME_MID);
                break;
            case MINIMUM_VOLUME: //最小音量
                vol_set(VOLUME_MIN);
                break;
            case CLOSE_THE_MUTE: //静音
            case TURN_ON_VOICE_BROADCAST: //开启语音播报
                prompt_player_enable(ENABLE);
                break;
            case MUTE: //静音
            case TURN_OFF_VOICE_BROADCAST: //关闭语音播报
                prompt_player_enable(DISABLE);
                break;
            case EXIT_SPEECH_RECOGNITION: //退出语音识别
                set_state_enter_wakeup(100);
                return ret;
            case PLEASE_TURN_ON_THE_LIGHT: //开灯
                light_control(LIGHT_POWER_ON);
                break;
            case TURN_OFF_LIGHTS: //关灯
                light_control(LIGHT_POWER_OFF);
                break;
            case TURN_UP_THE_LIGHT: //亮一点
                light_control(LIGHT_BRIGHT_INC);
                break;
            case DIM_THE_LIGHT: //暗一点
                light_control(LIGHT_BRIGHT_DEC);
                break;
            case MAXIMUM_BRIGHTNESS_OF_LIGHT: //最高亮度
                light_control(LIGHT_BRIGHT_MAX);
                break;
            case MODERATE_BRIGHTNESS: //中等亮度
                light_control(LIGHT_BRIGHT_MID);
                break;
            case MINIMUM_BRIGHTNESS_OF_LIGHT: //最低亮度
                light_control(LIGHT_BRIGHT_MIN);
                break;
            default:
                ret = 0;
                break;
        }
    }
#if PLAY_OTHER_CMD_EN
    if (ret)
    {
        pause_voice_in();
        prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback, true);
    }
#endif
    return ret;
}

/**
 * @brief 按命令词id响应asr消息处理
 *
 * @param asr_msg
 * @param cmd_handle
 * @param cmd_id
 * @return uint32_t
 */
uint32_t deal_asr_msg_by_cmd_id(sys_msg_asr_data_t *asr_msg, cmd_handle_t cmd_handle, uint16_t cmd_id)
{
    uint32_t ret = 1;
    int select_index = -1;
    switch (cmd_id)
    {
        ///tag-asr-msg-deal-by-cmd-id-start
        case 16: //改变颜色
            light_control(LIGHT_SWITCH_COLOR);
            break;
        case 17: //白光模式
            light_control(LIGHT_COLOR_WHITE);
            break;
        case 18: //冷色模式
            light_control(LIGHT_COLOR_COOL);
            break;
        case 19: //暖色模式
            light_control(LIGHT_COLOR_WARM);
            break;
        case 20: //闪光模式
            light_control(LIGHT_MODE_FLASH);
            break;
        case 21: //呼吸模式
            light_control(LIGHT_MODE_BREATH);
            break;
        case 22: //彩虹模式
            light_control(LIGHT_MODE_RAINBOW);
            break;
        ///tag-asr-msg-deal-by-cmd-id-end
        default:
            ret = 0;
            break;
    }
#if PLAY_OTHER_CMD_EN
    if (ret && select_index >= -1)
    {
        pause_voice_in();
        prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback, true);
    }
#endif
    return ret;
}

/**
 * @brief 用户自定义消息处理
 *
 * @param msg
 * @return uint32_t
 */
uint32_t deal_userdef_msg(sys_msg_t *msg)
{
    uint32_t ret = 1;
    switch (msg->msg_type)
    {
        /* 按键消息 */
        case SYS_MSG_TYPE_KEY:
        {
            sys_msg_key_data_t *key_rev_data;
            key_rev_data = &msg->msg_data.key_data;
            userapp_deal_key_msg(key_rev_data);
            break;
        }
#if MSG_COM_USE_UART_EN
        /* CI串口协议消息 */
        case SYS_MSG_TYPE_COM:
        {
#if ((UART_PROTOCOL_VER == 1) || (UART_PROTOCOL_VER == 2))
            sys_msg_com_data_t *com_rev_data;
            com_rev_data = &msg->msg_data.com_data;
            userapp_deal_com_msg(com_rev_data);
#endif
            break;
        }
#endif
    /* CI IIC 协议消息 */
#if MSG_USE_I2C_EN
        case SYS_MSG_TYPE_I2C:
        {
            sys_msg_i2c_data_t *i2c_rev_data;
            i2c_rev_data = &msg->msg_data.i2c_data;
            userapp_deal_i2c_msg(i2c_rev_data);
            break;
        }
#endif
        default:
            break;
    }
    return ret;
}
