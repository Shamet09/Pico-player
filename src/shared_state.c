#include "shared_state.h"

#include <string.h>

#include "pico/multicore.h"

shared_state_t g_shared;

void pp_shared_init(void)
{
    memset(&g_shared, 0, sizeof(g_shared));
    critical_section_init(&g_shared.lock);
    g_shared.sd_status = SD_STATUS_UNKNOWN;
    g_shared.volume    = VOLUME_DEFAULT;
    g_shared.playing   = false;   /* all'avvio si parte in pausa */
}

void pp_send_cmd(pp_cmd_t cmd, int32_t param)
{
    multicore_fifo_push_blocking((uint32_t)cmd);
    multicore_fifo_push_blocking((uint32_t)param);
}
