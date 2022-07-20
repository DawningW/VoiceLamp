#include "light.h"

#if LIGHT_TYPE == LIGHT_IR

#include "FreeRTOS.h"
#include "task.h"
#include "ci_log.h"
#include "ir_remote_driver.h"
#include "ir_data.h"

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))
#define MAX_BRIGHTNESS 9
#define MID_BRIGHTNESS 3

// 前两个字节为地址码(小端序), 第三个字节为命令码, 第四个字节为命令码的反码
typedef uint8_t nec_key_t[4];

// 注释是遥控器上标的按键名
nec_key_t KEY_POWER_ON = { 0x00, 0xEF, 0x03, 0xFC }; // ON
nec_key_t KEY_POWER_OFF = { 0x00, 0xEF, 0x02, 0xFD }; // OFF
nec_key_t KEY_BRIGHT_INC = { 0x00, 0xEF, 0x00, 0xFF }; // BRIGHT
nec_key_t KEY_BRIGHT_DEC = { 0x00, 0xEF, 0x01, 0xFE }; // DARK
nec_key_t KEY_COLORS[] =
{
        { 0x00, 0xEF, 0x07, 0xF8 }, // W

        { 0x00, 0xEF, 0x04, 0xFB }, // R
        { 0x00, 0xEF, 0x08, 0xF7 },
        { 0x00, 0xEF, 0x0C, 0xF3 },
        { 0x00, 0xEF, 0x10, 0xEF },
        { 0x00, 0xEF, 0x14, 0xEB },

        { 0x00, 0xEF, 0x05, 0xFA }, // G
        { 0x00, 0xEF, 0x09, 0xF6 },
        { 0x00, 0xEF, 0x0D, 0xF2 },
        { 0x00, 0xEF, 0x11, 0xEE },
        { 0x00, 0xEF, 0x15, 0xEA },

        { 0x00, 0xEF, 0x06, 0xF9 }, // B
        { 0x00, 0xEF, 0x0A, 0xF5 },
        { 0x00, 0xEF, 0x0E, 0xF1 },
        { 0x00, 0xEF, 0x12, 0xED },
        { 0x00, 0xEF, 0x16, 0xE9 },
};
nec_key_t KEY_MODE_FLASH = { 0x00, 0xEF, 0x0B, 0xF4 }; // FLASH
nec_key_t KEY_MODE_BREATH = { 0x00, 0xEF, 0x0F, 0xF0 }; // STROBE
nec_key_t KEY_MODE_RAINBOW = { 0x00, 0xEF, 0x13, 0xEC }; // FADE
nec_key_t KEY_MODE_SMOOTH = { 0x00, 0xEF, 0x17, 0xE8 }; // SMOOTH

uint8_t current_color = 0;

// TODO 我只想用send_nec_key, 但是这个函数在libir_lib.a这个库里, 看不到代码
int device_common_sendmsg(void *msgtyp, void *pDevice, void *pKey, void *data)
{
    return RETURN_OK;
}

// TODO 在换成自己写的send_nec_key之后换成发送重复码而非重复发送
int send_nec_key_repeat(nec_key_t nec_key, uint8_t repeat)
{
    for (uint8_t i = 0; i < repeat; i++)
    {
        if (send_nec_key(nec_key) != RETURN_OK)
        {
            return RETURN_ERR;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return RETURN_OK;
}

int light_init(void)
{
    int ret = RETURN_ERR;

    stIrPinInfo irPinInfo = {0};
    // config outpin
    irPinInfo.outPin.PinName  = PWM3_PAD;
    irPinInfo.outPin.GpioBase = GPIO1;
    irPinInfo.outPin.PinNum   = gpio_pin_4;
    irPinInfo.outPin.PwmFun   = SECOND_FUNCTION;
    irPinInfo.outPin.IoFun    = FIRST_FUNCTION;
    irPinInfo.outPin.PwmBase  = PWM3;
    // config revpin
    irPinInfo.revPin.PinName  = PWM4_PAD;
    irPinInfo.revPin.GpioBase = GPIO1;
    irPinInfo.revPin.PinNum   = gpio_pin_5;
    irPinInfo.revPin.IoFun    = FIRST_FUNCTION;
    irPinInfo.revPin.GpioIRQ  = GPIO1_IRQn;
    // config timer
    irPinInfo.irTimer.ir_use_timer     = TIMER0;
    irPinInfo.irTimer.ir_use_timer_IRQ = TIMER0_IRQn;
    // set io info
    ret = ir_init(&irPinInfo);
    if (ret != RETURN_OK)
    {
        ci_logerr(LOG_USER, "ir init failed!\n");
        return RETURN_ERR;
    }
    ir_hw_init();

    return RETURN_OK;
}

int light_deinit(void)
{
    // Nothing to do
    return RETURN_OK;
}

int light_control(LightCommand cmd)
{
    int ret = RETURN_ERR;
    switch (cmd)
    {
        case LIGHT_POWER_ON:
            ret = send_nec_key(KEY_POWER_ON);
            break;
        case LIGHT_POWER_OFF:
            ret = send_nec_key(KEY_POWER_OFF);
            break;
        case LIGHT_BRIGHT_INC:
            ret = send_nec_key(KEY_BRIGHT_INC);
            break;
        case LIGHT_BRIGHT_DEC:
            ret = send_nec_key(KEY_BRIGHT_DEC);
            break;
        case LIGHT_BRIGHT_MAX:
            ret = send_nec_key_repeat(KEY_BRIGHT_INC, MAX_BRIGHTNESS);
            break;
        case LIGHT_BRIGHT_MID:
            ret = send_nec_key_repeat(KEY_BRIGHT_DEC, MAX_BRIGHTNESS)
                | send_nec_key_repeat(KEY_BRIGHT_INC, MID_BRIGHTNESS);
            break;
        case LIGHT_BRIGHT_MIN:
            ret = send_nec_key_repeat(KEY_BRIGHT_DEC, MAX_BRIGHTNESS);
            break;
        case LIGHT_SWITCH_COLOR:
            ret = send_nec_key(KEY_COLORS[current_color]);
            if (++current_color >= ARRAY_LENGTH(KEY_COLORS))
                current_color = 0;
            break;
        case LIGHT_COLOR_WHITE:
            ret = send_nec_key(KEY_COLORS[0]);
            break;
        case LIGHT_COLOR_COOL:
            ret = send_nec_key(KEY_COLORS[8]);
            break;
        case LIGHT_COLOR_WARM:
            ret = send_nec_key(KEY_COLORS[4]);
            break;
        case LIGHT_MODE_FLASH:
            ret = send_nec_key(KEY_MODE_FLASH);
            break;
        case LIGHT_MODE_BREATH:
            ret = send_nec_key(KEY_MODE_BREATH);
            break;
        case LIGHT_MODE_RAINBOW:
            ret = send_nec_key(KEY_MODE_RAINBOW);
            break;
    }
    return ret;
}

#endif
