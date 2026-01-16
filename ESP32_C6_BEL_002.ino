#include "Arduino.h"
#include "Zigbee.h"
#include "driver/i2s_std.h"
#include "sounddata.h"
#include <Preferences.h>

#define I2S_BCLK GPIO_NUM_6
#define I2S_WS   GPIO_NUM_7
#define I2S_DIN  GPIO_NUM_5

#define ENDPOINT_ID 1
ZigbeeDimmableLight zbBell(ENDPOINT_ID); 

i2s_chan_handle_t tx_handle = NULL;
Preferences preferences;

float current_volume = 0.15; 
uint8_t last_zigbee_level = 77; 

void setup_i2s() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 128;
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(12000), 
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = I2S_BCLK, .ws = I2S_WS, .dout = I2S_DIN, .din = I2S_GPIO_UNUSED },
    };
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

void play_bell() {
    if (tx_handle == NULL) return;

    size_t bytes_written;
    int num_samples = bell_sound_len / sizeof(int16_t);
    int herhalingen = 3; 

    for (int h = 0; h < herhalingen; h++) {
        Serial.printf("Ronde %d met volume %.2f\n", h + 1, current_volume);
        
        i2s_channel_enable(tx_handle); // Hardware AAN

        for (int i = 0; i < num_samples; i++) {
            int16_t sample = bell_sound[i]; 
            int16_t adjusted = (int16_t)(sample * current_volume);
            i2s_channel_write(tx_handle, &adjusted, sizeof(adjusted), &bytes_written, portMAX_DELAY);
        }
        
        // 1. Schrijf een kort blokje stilte om de 'hik' weg te drukken
        int16_t silence = 0;
        for (int s = 0; s < 400; s++) {
            i2s_channel_write(tx_handle, &silence, sizeof(silence), &bytes_written, portMAX_DELAY);
        }

        // 2. Hardware UIT zetten vóór de pauze (dit voorkomt overlap)
        i2s_channel_disable(tx_handle); 

        // 3. De pauze tussen de belsignalen
        if (h < herhalingen - 1) {
            delay(800); // 0,8 seconde echte stilte
        }
    }
    
    zbBell.setLight(false, last_zigbee_level);
}

void on_light_change(bool state, uint8_t level) {
    // Volume bijwerken
    if (level > 0) {
        last_zigbee_level = level;
        // Schaal terug naar 0.0 - 0.5 (max 50%) voor veilig volume
        current_volume = (float)level / 510.0; 
        
        preferences.begin("bell", false);
        preferences.putFloat("volume", current_volume);
        preferences.putUChar("level", last_zigbee_level);
        preferences.end();
    }

    if (state) {
        play_bell();
    }
}

void setup() {
    Serial.begin(115200);
    setup_i2s();

    preferences.begin("bell", true);
    current_volume = preferences.getFloat("volume", 0.15);
    last_zigbee_level = preferences.getUChar("level", 77);
    preferences.end();

    zbBell.onLightChange(on_light_change);
    Zigbee.addEndpoint(&zbBell);

    if (!Zigbee.begin()) {
        while (1) delay(10);
    }
    
    zbBell.setLight(false, last_zigbee_level);
    Serial.println("Systeem klaar.");
}

void loop() {
    delay(10); 
}