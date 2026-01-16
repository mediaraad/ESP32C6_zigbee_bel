#include "Arduino.h"
#include "Zigbee.h"
#include "driver/i2s_std.h"
#include "sounddata.h"
#include <Preferences.h>

#define I2S_BCLK GPIO_NUM_6
#define I2S_WS   GPIO_NUM_7
#define I2S_DIN  GPIO_NUM_5

/* --- ZIGBEE CONFIGURATIE --- */
ZigbeeDimmableLight zbBell(1);   // Endpoint 1: Volume & Aan/Uit
ZigbeeDimmableLight zbPause(2);  // Endpoint 2: Pauze instelling

i2s_chan_handle_t tx_handle = NULL;
Preferences preferences;

// Variabelen met standaardwaarden
float current_volume = 0.15;
uint8_t last_vol_level = 77;
int pause_ms = 800;
uint8_t last_pause_level = 80; // 80 * 10 = 800ms

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
        i2s_channel_enable(tx_handle);
        for (int i = 0; i < num_samples; i++) {
            int16_t sample = bell_sound[i]; 
            int16_t adjusted = (int16_t)(sample * current_volume);
            i2s_channel_write(tx_handle, &adjusted, sizeof(adjusted), &bytes_written, portMAX_DELAY);
        }
        
        int16_t silence = 0;
        for (int s = 0; s < 400; s++) {
            i2s_channel_write(tx_handle, &silence, sizeof(silence), &bytes_written, portMAX_DELAY);
        }

        i2s_channel_disable(tx_handle); 

        if (h < herhalingen - 1) {
            Serial.printf("Pauze: %d ms\n", pause_ms);
            delay(pause_ms); 
        }
    }
    zbBell.setLight(false, last_vol_level);
}

// Callback voor Volume (Endpoint 1)
void on_vol_change(bool state, uint8_t level) {
    if (level > 0) {
        last_vol_level = level;
        current_volume = (float)level / 510.0;
        preferences.begin("bell", false);
        preferences.putFloat("volume", current_volume);
        preferences.putUChar("vol_lev", last_vol_level);
        preferences.end();
    }
    if (state) play_bell();
}

// Callback voor Pauze (Endpoint 2)
void on_pause_change(bool state, uint8_t level) {
    // We gebruiken level 0-255 als 0-2550ms (stapjes van 10ms)
    last_pause_level = level;
    pause_ms = level * 10;
    
    preferences.begin("bell", false);
    preferences.putInt("pause", pause_ms);
    preferences.putUChar("pau_lev", last_pause_level);
    preferences.end();
    
    Serial.printf("Nieuwe pauze ingesteld: %d ms\n", pause_ms);
}

void setup() {
    Serial.begin(115200);
    setup_i2s();

    // Laad alle waarden uit het geheugen
    preferences.begin("bell", true);
    current_volume = preferences.getFloat("volume", 0.15);
    last_vol_level = preferences.getUChar("vol_lev", 77);
    pause_ms = preferences.getInt("pause", 800);
    last_pause_level = preferences.getUChar("pau_lev", 80);
    preferences.end();

    // Koppel callbacks
    zbBell.onLightChange(on_vol_change);
    zbPause.onLightChange(on_pause_change);
    
    // Voeg beide endpoints toe
    Zigbee.addEndpoint(&zbBell);
    Zigbee.addEndpoint(&zbPause);

    if (!Zigbee.begin()) {
        while (1) delay(10);
    }
    
    zbBell.setLight(false, last_vol_level);
    zbPause.setLight(false, last_pause_level);
    Serial.println("Systeem klaar. Twee schuifbalken beschikbaar.");
}

void loop() {
    delay(10); 
}