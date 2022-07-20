#include "light.h"

#if LIGHT_TYPE == LIGHT_PWM

int light_init(void)
{
    return RETURN_OK;
}

int light_deinit(void)
{
    // Nothing to do
    return RETURN_OK;
}

int light_control(LightCommand cmd)
{
    return RETURN_OK;
}

#endif
