#include "Arduino.h"
#include "Zigbee.h"
#include "driver/i2s_std.h"
#include "sounddata.h" // <--- Voeg deze regel toe

/* --- I2S PIN CONFIGURATIE --- */
#define I2S_BCLK GPIO_NUM_6
#define I2S_WS   GPIO_NUM_7
#define I2S_DIN  GPIO_NUM_5

/* --- ZIGBEE CONFIGURATIE --- */
#define ENDPOINT_ID 1
ZigbeeLight zbBell(ENDPOINT_ID); 

i2s_chan_handle_t tx_handle = NULL;

void setup_i2s() {
    // We voegen dma_desc_num en dma_frame_num toe om de buffer klein te houden
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;    // Minder buffers
    chan_cfg.dma_frame_num = 128; // Kleinere buffers per keer
    
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    
    // De rest van je std_cfg blijft hetzelfde...
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(12000), 
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = I2S_BCLK, .ws = I2S_WS, .dout = I2S_DIN, .din = I2S_GPIO_UNUSED },
    };
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);
}

void play_bell() {
    if (tx_handle == NULL) return;

    float volume = 0.2; 
    size_t bytes_written;
    int num_samples = bell_sound_len / sizeof(int16_t);
    int herhalingen = 3; 

    for (int h = 0; h < herhalingen; h++) {
        Serial.printf("Ronde %d...\n", h + 1);

        // 1. Speel het geluid af
        for (int i = 0; i < num_samples; i++) {
            int16_t original_sample = pgm_read_word(&bell_sound[i]);
            int16_t adjusted_sample = (int16_t)(original_sample * volume);
            i2s_channel_write(tx_handle, &adjusted_sample, sizeof(adjusted_sample), &bytes_written, portMAX_DELAY);
        }

        // 2. VOEG STILTE TOE TUSSEN DE HERHALINGEN (tegen de tik)
        // We sturen 1000 samples stilte (bij 8000Hz is dit ~125ms pauze)
        int16_t silence = 0;
        for (int s = 0; s < 1000; s++) {
            i2s_channel_write(tx_handle, &silence, sizeof(silence), &bytes_written, portMAX_DELAY);
        }
        
        // Optioneel: extra rusttijd voor het gehoor
        delay(100); 
    }

    // 3. Laatste check: zet status terug
    delay(100); 
    zbBell.setLight(false);
    Serial.println("Klaar.");
}


// Callback functie
void on_light_change(bool state) {
    if (state) {
        Serial.println("Zigbee: AAN ontvangen. De bel gaat!");
        play_bell();
        
        // In versie 3.3.5 is dit zeer waarschijnlijk setLight
        // Mocht dit ook een fout geven, vervang het dan door: zbBell.lightOff();
        zbBell.setLight(false); 
    }
}

void setup() {
    Serial.begin(115200);
    setup_i2s();

    // De compiler bevestigde eerder dat dit de juiste naam is
    zbBell.onLightChange(on_light_change);
    
    Zigbee.addEndpoint(&zbBell);

    Serial.println("Zigbee Bel starten...");
    if (!Zigbee.begin()) {
        Serial.println("Zigbee start mislukt!");
        while (1) {
            delay(10);
        }
    }
    Serial.println("Succes! Klaar om te koppelen.");
}

void loop() {
    delay(10); 
}