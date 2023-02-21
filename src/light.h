#ifndef _LIGHT_H
#define _LIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

// 夜灯种类
#define LIGHT_NONE 0
#define LIGHT_PWM 1
#define LIGHT_RGB 2
#define LIGHT_IR 3
// 在此选择夜灯种类
#define LIGHT_TYPE LIGHT_RGB
#if (LIGHT_TYPE != LIGHT_PWM && LIGHT_TYPE != LIGHT_RGB && LIGHT_TYPE != LIGHT_IR)
#error "Please choose light type first!"
#endif

typedef enum {
    LIGHT_POWER_ON,  // 开灯
    LIGHT_POWER_OFF, // 关灯

    LIGHT_BRIGHT_INC, // 增加亮度
    LIGHT_BRIGHT_DEC, // 降低亮度
    LIGHT_BRIGHT_MAX, // 最高亮度
    LIGHT_BRIGHT_MID, // 中等亮度
    LIGHT_BRIGHT_MIN, // 最低亮度

    LIGHT_SWITCH_COLOR, // 改变颜色
    LIGHT_COLOR_WHITE,  // 白光
    LIGHT_COLOR_COOL,   // 冷色
    LIGHT_COLOR_WARM,   // 暖色

    LIGHT_MODE_FLASH,   // 闪光模式
    LIGHT_MODE_BREATH,  // 呼吸模式
    LIGHT_MODE_RAINBOW, // 彩虹模式

    LIGHT_CMD_COUNT // 命令数量
} LightCommand;

/**
 * @brief 初始化小夜灯
 */
int light_init(void);
/**
 * @brief 释放小夜灯
 */
int light_deinit(void);
/**
 * @brief 向小夜灯发送控制命令
 */
int light_control(LightCommand cmd);

#ifdef __cplusplus
}
#endif

#endif
