#ifndef APPLICATION_PAGE_COMMON_H
#define APPLICATION_PAGE_COMMON_H

#include "hal/display_hal.h"
#include "hal/touch_hal.h"

typedef struct {
    void (*draw)(void);
    void (*handle_click)(int x, int y, int event_type);
} app_page_t;

app_page_t *page_home_get_instance(void);
app_page_t *page_ai_get_instance(void);
app_page_t *page_weather_get_instance(void);
int weather_page_request_refresh_async(int force_refresh);
app_page_t *page_album_get_instance(void);
app_page_t *page_file_get_instance(void);
app_page_t *page_settings_get_instance(void);

#endif // APPLICATION_PAGE_COMMON_H