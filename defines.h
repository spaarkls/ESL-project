#ifndef DEFINES_H__
#define DEFINES_H__

#include <stddef.h>

#define PWM_TOP_VALUE   ( 1000 )

#define FLASH_ADDRESS                   ( 0x7F000 )
#define FLASH_CONSTRAINTED_MAX_ADDRESS  ( 0x80000 ) 
#define FLASH_CONSTRAINTED_MIN_ADDRESS  ( 0x1c000 )

#define MAX_VALUE_HUE           ( 360 )
#define MAX_VALUE_SATURATION    ( 100 )
#define MAX_VALUE_BRIGHTNESS    ( 100 )


#define DELAY_DEBOUNCE_MS       ( 200 )
#define DELAY_DOUBLE_CLICK_MS   ( 400 )
#define DELAY_INTERVAL_FADE_MS  ( 20 )

#define STEP_CHANGE_HUE         ( 2 )
#define STEP_CHANGE_SATURATION  ( 1 )
#define STEP_CHANGE_BRIGHT      ( 1 )

#define STEP_CHANGE_YELLOW_COLOR_FAST ( 50 )
#define STEP_CHANGE_YELLOW_COLOR_SLOW ( 25 )

#define VALID_PTR(x) if ( ( x ) == NULL ) for (;;)
#define UNUSED(x) (void)(x)


#endif // DEFINES_H__