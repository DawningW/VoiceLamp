#include "light.h"

#if LIGHT_TYPE == LIGHT_PWM

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "ci112x_scu.h"
#include "ci112x_gpio.h"
#include "ci112x_pwm.h"
//#include "color_light_control.h" // 启英泰伦自带的RGB彩灯驱动
//#include "led_light_control.h" // 启英泰伦自带的PWM夜灯及眨眼灯驱动

int light_init(void)
{
    // 以下给出PWM5的初始化示例, 请自行按照需求实现PWM初始化
    Scu_SetDeviceGate(HAL_GPIO1_BASE, ENABLE);
    Scu_SetIOReuse(PWM5_PAD, SECOND_FUNCTION);
    Scu_SetDeviceGate(HAL_PWM5_BASE, ENABLE);
    pwm_init_t pwm_config;
    pwm_config.clk_sel = 0;
    pwm_config.freq = 25000;
    pwm_config.duty = 0;
    pwm_config.duty_max = 100;
    pwm_init((pwm_base_t) HAL_PWM5_BASE, pwm_config);
    pwm_start((pwm_base_t) HAL_PWM5_BASE);
    return RETURN_OK;
}

int light_deinit(void)
{
    // Nothing to do
    return RETURN_OK;
}

int light_control(LightCommand cmd)
{
    // 请自行实现灯光命令回调
    return RETURN_ERR;
}

#endif
