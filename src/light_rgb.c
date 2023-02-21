#include "light.h"

#if LIGHT_TYPE == LIGHT_RGB

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
//#include "semphr.h"
#include "timers.h"
#include "ci112x_scu.h"
#include "ci112x_core_eclic.h"
#include "ci112x_core_misc.h"
#include "ci112x_timer.h"
#include "ci112x_gpio.h"
#include "ci_nvdata_manage.h"

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))
#define TIMER_US (get_apb_clk() / 1000000) // 94
#define NVDATA_ID_LIGHT NVDATA_ID_USER_START
#define LIGHT_COUNT 2
#define MAX_BRIGHTNESS 8

typedef enum {
    MODE_OFF,     // 关闭
    MODE_NORMAL,  // 常亮
    MODE_FLASH,   // 闪光模式
    MODE_BREATH,  // 呼吸模式
    MODE_RAINBOW, // 彩虹模式
    MODE_COUNT    // 模式数量
} LightMode;

const uint32_t COLORS[] =
{
        0xFF0000,
        0x00FF00,
        0x0000FF,
        0xFF00FF,
        0x00FFFF,
        0xFFFF00,
        0xFFFFFF
};

//SemaphoreHandle_t timer_lock;
TimerHandle_t rgb_timer;
struct
{
    bool power;
    LightMode mode;
    uint32_t color;
    uint8_t brightness;
} config;
int8_t color_index = 0;
uint8_t tick = 0;
uint16_t hue = 0;

static void hex2rgb(uint32_t hex, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (hex & 0xFF0000) >> 16;
    *g = (hex & 0x00FF00) >> 8;
    *b = (hex & 0x0000FF);
}

/**
 * @brief 将HSV颜色空间转换为RGB颜色空间
 *      - 因为HSV使用起来更加直观、方便，所以代码逻辑部分使用HSV。但WS2812B RGB-LED灯珠的驱动使用的是RGB，所以需要转换。
 *
 * @param  h HSV颜色空间的H：色调。单位°，范围0~360。（Hue 调整颜色，0°-红色，120°-绿色，240°-蓝色，以此类推）
 * @param  s HSV颜色空间的S：饱和度。单位%，范围0~100。（Saturation 饱和度高，颜色深而艳；饱和度低，颜色浅而发白）
 * @param  v HSV颜色空间的V：明度。单位%，范围0~100。（Value 控制明暗，明度越高亮度越亮，越低亮度越低）
 * @param  r RGB-R值的指针
 * @param  g RGB-G值的指针
 * @param  b RGB-B值的指针
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
static void hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

/**
 * @brief 延时count个100ns
 */
static void _delay(uint16_t count)
{
    while (--count)
    {
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    }
}

// 定时器用不明白
//static void delay_ns(uint16_t ns)
//{
//    timer_set_count(TIMER0, ns * TIMER_US / 1000);
//    timer_start(TIMER0);
////    xSemaphoreTake(timer_lock, portMAX_DELAY);
//    unsigned int count;
//    do
//    {
//        timer_get_count(TIMER0, &count);
//    }
//    while (count > 0);
//}

//static void timer_handler(void)
//{
//    xSemaphoreGiveFromISR(timer_lock, NULL);
//}

static void rgb_send_byte(uint8_t byte)
{
    for (int8_t i = 7; i >= 0; i--)
    {
        gpio_set_output_level_single(GPIO1, gpio_pin_6, 1);
        _delay(3);
        gpio_set_output_level_single(GPIO1, gpio_pin_6, byte & (0x01 << i));
        _delay(7);
        gpio_set_output_level_single(GPIO1, gpio_pin_6, 0);
        _delay(3);
    }
}

static void rgb_send(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < LIGHT_COUNT; i++)
    {
        rgb_send_byte(g); // green
        rgb_send_byte(r); // red
        rgb_send_byte(b); // blue
    }
//    gpio_set_output_level_single(GPIO1, gpio_pin_6, 0);
//    vTaskDelay(pdMS_TO_TICKS(1));
}

// 灯太亮了刺眼睛, 限制最大亮度
#define _MAX_BRIGHTNESS (MAX_BRIGHTNESS * 2)

static void rgb_send_hex(uint32_t color)
{
    uint8_t r = ((color & 0xFF0000) >> 16) * config.brightness / _MAX_BRIGHTNESS;
    uint8_t g = ((color & 0x00FF00) >> 8) * config.brightness / _MAX_BRIGHTNESS;
    uint8_t b = (color & 0x0000FF) * config.brightness / _MAX_BRIGHTNESS;
    rgb_send(r, g, b);
}

static void rgb_send_hsv(uint32_t h, uint32_t s, uint32_t v)
{
    uint8_t r, g, b;
    v = v * config.brightness / _MAX_BRIGHTNESS;
    hsv2rgb(h, s, v, &r, &g, &b);
    rgb_send(r, g, b);
}

#define rgb_clear() rgb_send(0, 0, 0)

static int rgb_update(LightMode new_mode)
{
    if (new_mode == MODE_OFF)
    {
        rgb_clear(); // 防止信号出错关不掉灯
        if (config.power)
        {
            xTimerStop(rgb_timer, 0);
            config.power = false;
        }
    }
    else
    {
        if (new_mode == MODE_NORMAL)
        {
            if (config.power && config.mode != MODE_NORMAL)
            {
                xTimerStop(rgb_timer, 0);
            }
            rgb_send_hex(config.color);
        }
        else
        {
            if (!config.power || config.mode == MODE_NORMAL)
            {
                xTimerStart(rgb_timer, 0);
            }
        }
        config.power = true;
        config.mode = new_mode;
    }
    cinv_item_write(NVDATA_ID_LIGHT, sizeof(config), &config);
    return RETURN_OK;
}

static void rgb_timer_handler(TimerHandle_t timer)
{
    if (config.mode == MODE_FLASH)
    {
        if (++tick >= 10)
        {
            tick = 0;
            rgb_send_hex(COLORS[color_index++]);
            if (color_index >= ARRAY_LENGTH(COLORS))
            {
                color_index = 0;
            }
        }
    }
    else if (config.mode == MODE_BREATH)
    {
        if (tick < 20)
        {
            uint32_t color = config.color;
            uint8_t r, g, b;
            hex2rgb(config.color, &r, &g, &b);
            float x = (float) tick / 20;
            int scale = -1010 * x * x + 1010 * x;
            r = r * scale / 255 * config.brightness / _MAX_BRIGHTNESS;
            g = g * scale / 255 * config.brightness / _MAX_BRIGHTNESS;
            b = b * scale / 255 * config.brightness / _MAX_BRIGHTNESS;
            rgb_send(r, g, b);
        }
        else
        {
            rgb_clear();
        }
        if (++tick >= 40)
            tick = 0;
    }
    else if (config.mode == MODE_RAINBOW)
    {
        rgb_send_hsv(hue, 100, 100);
        hue += 5;
        if (hue >= 360)
            hue = 0;
    }
}

int light_init(void)
{
    // 从nvdata里读取灯效设置
    uint16_t real_len;
    if (cinv_item_read(NVDATA_ID_LIGHT, sizeof(config), &config, &real_len) != CINV_OPER_SUCCESS)
    {
        config.power = true;
        config.mode = MODE_NORMAL;
        config.color = 0xFFFFFF;
        config.brightness = MAX_BRIGHTNESS / 2;
        cinv_item_init(NVDATA_ID_LIGHT, sizeof(config), &config);
    }
    // 初始化 GPIO1[6]
    Scu_SetDeviceGate(HAL_GPIO1_BASE, ENABLE);
    Scu_SetIOReuse(PWM5_PAD, FIRST_FUNCTION);
    gpio_set_output_mode(GPIO1, gpio_pin_6);
    gpio_set_output_level_single(GPIO1, gpio_pin_6, 0);
    // 初始化 TIMER0
    Scu_SetDeviceGate(TIMER0, ENABLE);
    Scu_Setdevice_Reset(TIMER0);
    Scu_Setdevice_ResetRelease(TIMER0);
//    __eclic_irq_set_vector(TIMER0_IRQn, (int32_t) timer_handler);
//    eclic_irq_enable(TIMER0_IRQn);
//    timer_init_t init;
//    init.mode = timer_count_mode_single;
//    init.div = timer_clk_div_0;
//    init.width = timer_iqr_width_2;
//    init.count = 0xFFFFFFFF;
//    timer_init(TIMER0, init);
//    timer_lock = xSemaphoreCreateCounting(1, 0);
//    if (timer_lock == NULL)
//    {
//        return RETURN_ERR;
//    }
    // 50毫秒刷新一次
    rgb_timer = xTimerCreate("rgb_timer", pdMS_TO_TICKS(50), pdTRUE, (void*) 1, rgb_timer_handler);
    if (rgb_timer == NULL)
    {
        return RETURN_ERR;
    }
    // 更新灯光效果
    LightMode mode = config.mode;
    config.mode = MODE_OFF;
    rgb_update(mode);
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
            ret = rgb_update(config.mode);
            break;
        case LIGHT_POWER_OFF:
            ret = rgb_update(MODE_OFF);
            break;
        case LIGHT_BRIGHT_INC:
            config.brightness++;
            if (config.brightness > MAX_BRIGHTNESS)
                config.brightness = MAX_BRIGHTNESS;
            ret = rgb_update(config.mode);
            break;
        case LIGHT_BRIGHT_DEC:
            config.brightness--;
            if (config.brightness < 1)
                config.brightness = 1;
            ret = rgb_update(config.mode);
            break;
        case LIGHT_BRIGHT_MAX:
            config.brightness = MAX_BRIGHTNESS;
            ret = rgb_update(config.mode);
            break;
        case LIGHT_BRIGHT_MID:
            config.brightness = MAX_BRIGHTNESS / 2;
            ret = rgb_update(config.mode);
            break;
        case LIGHT_BRIGHT_MIN:
            config.brightness = 1;
            ret = rgb_update(config.mode);
            break;
        case LIGHT_SWITCH_COLOR:
            config.color = COLORS[color_index++];
            if (color_index >= ARRAY_LENGTH(COLORS))
            {
                color_index = 0;
            }
            ret = rgb_update(MODE_NORMAL);
            break;
        case LIGHT_COLOR_WHITE:
            config.color = 0xFFFFFF;
            ret = rgb_update(MODE_NORMAL);
            break;
        case LIGHT_COLOR_COOL:
            config.color = 0x88AAFF;
            ret = rgb_update(MODE_NORMAL);
            break;
        case LIGHT_COLOR_WARM:
            config.color = 0xFF8888;
            ret = rgb_update(MODE_NORMAL);
            break;
        case LIGHT_MODE_FLASH:
            ret = rgb_update(MODE_FLASH);
            break;
        case LIGHT_MODE_BREATH:
            ret = rgb_update(MODE_BREATH);
            break;
        case LIGHT_MODE_RAINBOW:
            ret = rgb_update(MODE_RAINBOW);
            break;
    }
    return ret;
}

#endif
