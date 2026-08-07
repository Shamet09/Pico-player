/* hw_config.c - configurazione hardware per no-OS-FatFS-SD-SPI-RPi-Pico.
 *
 * La libreria si aspetta che sia l'applicazione a fornire questi array e le
 * quattro funzioni di accesso: qui descriviamo l'unico slot microSD di
 * PicoPlayer, cablato su SPI1.
 *
 * I numeri dei pin arrivano da src/config.h, cosi' esiste una sola fonte di
 * verita' per la piedinatura.
 */
#include "hw_config.h"

#include "config.h"

/* Un elemento per ogni SPI hardware usata. */
static spi_t spis[] = {
    {
        .hw_inst   = spi1,
        .miso_gpio = PIN_SD_MISO,
        .mosi_gpio = PIN_SD_MOSI,
        .sck_gpio  = PIN_SD_SCK,
        .baud_rate = SD_BAUD_RATE,
        /* DMA_IRQ_num viene impostato dal driver in base alla scelta fatta con
         * set_spi_dma_irq_channel() in player.c: usiamo DMA_IRQ_1 perche'
         * pico_audio_i2s occupa gia' DMA_IRQ_0. */
    }
};

/* Un elemento per ogni slot microSD. */
static sd_card_t sd_cards[] = {
    {
        .pcName             = "0:",      /* "logical drive" di FatFs */
        .spi                = &spis[0],
        .ss_gpio            = PIN_SD_CS,
        .use_card_detect    = false,     /* il modulo usato non lo espone */
        .card_detect_gpio   = 0,
        .card_detected_true = 1,
    }
};

size_t sd_get_num(void)
{
    return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num)
{
    return (num < sd_get_num()) ? &sd_cards[num] : NULL;
}

size_t spi_get_num(void)
{
    return count_of(spis);
}

spi_t *spi_get_by_num(size_t num)
{
    return (num < spi_get_num()) ? &spis[num] : NULL;
}
