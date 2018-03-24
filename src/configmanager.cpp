/* configmanager persists runtime configuration using NVRAM of ESP32*/

#include "main.h"
#include "globals.h"
#include <nvs.h>
#include <nvs_flash.h>

// Local logging tag
static const char *TAG = "configmanager";

nvs_handle my_handle;

esp_err_t err;

// defined in antenna.cpp
#ifdef HAS_ANTENNA_SWITCH
    void antenna_select(const int8_t _ant);
#endif

// populate cfg vars with factory settings
void defaultConfig() {
    cfg.lorasf      = LORASFDEFAULT; // 7-12, initial lora spreadfactor defined in main.h
    cfg.txpower     = 15; // 2-15, lora tx power
    cfg.adrmode     = 1;  // 0=disabled, 1=enabled
    cfg.screensaver = 0;  // 0=disabled, 1=enabled
    cfg.screenon    = 1;  // 0=disbaled, 1=enabled
    cfg.countermode = 0;  // 0=cyclic, 1=cumulative, 2=cyclic confirmed
    cfg.rssilimit   = 0;  // threshold for rssilimiter, negative value!
    cfg.wifiscancycle = SEND_SECS; // wifi scan cycle [seconds/2]
    cfg.wifichancycle = WIFI_CHANNEL_SWITCH_INTERVAL; // wifi channel switch cycle [seconds/100]
    cfg.blescancycle  = BLESCANTIME; // BLE scan cycle [seconds]
    cfg.blescan     = 0;  // 0=disabled, 1=enabled
    cfg.wifiant     = 0;  // 0=internal, 1=external (for LoPy/LoPy4)
    strncpy( cfg.version, PROGVERSION, sizeof(cfg.version)-1 );
}

void open_storage() {
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    ESP_LOGI(TAG, "Opening NVS");
    err = nvs_open("config", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) 
      ESP_LOGI(TAG, "Error (%d) opening NVS handle", err);
    else
      ESP_LOGI(TAG, "Done");
}

// erase all keys and values in NVRAM
void eraseConfig() {
    ESP_LOGI(TAG, "Clearing settings in NVS");
    open_storage(); 
    if (err == ESP_OK) {
      nvs_erase_all(my_handle);
      nvs_commit(my_handle);
      nvs_close(my_handle);
      ESP_LOGI(TAG, "Done");} 
    else { 
      ESP_LOGW(TAG, "NVS erase failed"); } 
}

// save current configuration from RAM to NVRAM
void saveConfig() {
    ESP_LOGI(TAG, "Storing settings in NVS");
    open_storage();
    if (err == ESP_OK) {
      int8_t  flash8  = 0;
      int16_t flash16 = 0;
      size_t required_size;
      char storedversion[10];
    
      if( nvs_get_str(my_handle, "version", storedversion, &required_size) != ESP_OK || strcmp(storedversion, cfg.version) != 0 )
        nvs_set_str(my_handle, "version", cfg.version);
      
      if( nvs_get_i8(my_handle, "lorasf", &flash8) != ESP_OK || flash8 != cfg.lorasf )
        nvs_set_i8(my_handle, "lorasf", cfg.lorasf);

      if( nvs_get_i8(my_handle, "txpower", &flash8) != ESP_OK || flash8 != cfg.txpower )
        nvs_set_i8(my_handle, "txpower", cfg.txpower);

      if( nvs_get_i8(my_handle, "adrmode", &flash8) != ESP_OK || flash8 != cfg.adrmode )
        nvs_set_i8(my_handle, "adrmode", cfg.adrmode);

      if( nvs_get_i8(my_handle, "screensaver", &flash8) != ESP_OK || flash8 != cfg.screensaver )
        nvs_set_i8(my_handle, "screensaver", cfg.screensaver);

      if( nvs_get_i8(my_handle, "screenon", &flash8) != ESP_OK || flash8 != cfg.screenon )
        nvs_set_i8(my_handle, "screenon", cfg.screenon);

      if( nvs_get_i8(my_handle, "countermode", &flash8) != ESP_OK || flash8 != cfg.countermode )
        nvs_set_i8(my_handle, "countermode", cfg.countermode);

      if( nvs_get_i8(my_handle, "wifiscancycle", &flash8) != ESP_OK || flash8 != cfg.wifiscancycle )
        nvs_set_i8(my_handle, "wifiscancycle", cfg.wifiscancycle);

      if( nvs_get_i8(my_handle, "wifichancycle", &flash8) != ESP_OK || flash8 != cfg.wifichancycle )
        nvs_set_i8(my_handle, "wifichancycle", cfg.wifichancycle);

      if( nvs_get_i8(my_handle, "blescancycle", &flash8) != ESP_OK || flash8 != cfg.blescancycle )
        nvs_set_i8(my_handle, "blescancycle", cfg.blescancycle);

      if( nvs_get_i8(my_handle, "blescanmode", &flash8) != ESP_OK || flash8 != cfg.blescan )
        nvs_set_i8(my_handle, "blescanmode", cfg.blescan);

      if( nvs_get_i8(my_handle, "wifiant", &flash8) != ESP_OK || flash8 != cfg.wifiant )
        nvs_set_i8(my_handle, "wifiant", cfg.wifiant);        

      if( nvs_get_i16(my_handle, "rssilimit", &flash16) != ESP_OK || flash16 != cfg.rssilimit )
        nvs_set_i16(my_handle, "rssilimit", cfg.rssilimit);
      
      err = nvs_commit(my_handle);
      nvs_close(my_handle);
      if ( err == ESP_OK ) {
        ESP_LOGI(TAG, "Done");
      } else {
        ESP_LOGW(TAG, "NVS config write failed");
      }
    } else {
      ESP_LOGW(TAG, "Error (%d) opening NVS handle", err);
    }
}

// set and save cfg.version
void migrateVersion() {
  sprintf(cfg.version, "%s", PROGVERSION);
  ESP_LOGI(TAG, "version set to %s", cfg.version);
  saveConfig();
}

// load configuration from NVRAM into RAM and make it current
void loadConfig() {
  defaultConfig(); // start with factory settings
  ESP_LOGI(TAG, "Reading settings from NVS");
  open_storage();
  if (err != ESP_OK) {
    ESP_LOGW(TAG,"Error (%d) opening NVS handle, storing defaults", err);
    saveConfig(); } // saves factory settings to NVRAM
  else {
    int8_t  flash8  = 0;
    int16_t flash16 = 0;
    size_t required_size;
    
    // check if configuration stored in NVRAM matches PROGVERSION
    if( nvs_get_str(my_handle, "version", NULL, &required_size) == ESP_OK ) {
      nvs_get_str(my_handle, "version", cfg.version, &required_size);
      ESP_LOGI(TAG, "NVRAM settings version = %s", cfg.version);
      if (strcmp(cfg.version, PROGVERSION)) {
        ESP_LOGI(TAG, "migrating NVRAM settings to new version %s", PROGVERSION);
        nvs_close(my_handle);
        migrateVersion();
      }
    } else {
        ESP_LOGI(TAG, "new version %s, deleting NVRAM settings", PROGVERSION);
        nvs_close(my_handle);
        eraseConfig();
        migrateVersion();
    }

    // overwrite defaults with valid values from NVRAM
    if( nvs_get_i8(my_handle, "lorasf", &flash8) == ESP_OK ) {
      cfg.lorasf = flash8;
      ESP_LOGI(TAG, "lorasf = %i", flash8);
    } else {
      ESP_LOGI(TAG, "lorasf set to default %i", cfg.lorasf);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "txpower", &flash8) == ESP_OK ) {
      cfg.txpower = flash8;
      ESP_LOGI(TAG, "txpower = %i", flash8);
    } else {
      ESP_LOGI(TAG, "txpower set to default %i", cfg.txpower);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "adrmode", &flash8) == ESP_OK ) {
      cfg.adrmode = flash8;
      ESP_LOGI(TAG, "adrmode = %i", flash8);
    } else {
      ESP_LOGI(TAG, "adrmode set to default %i", cfg.adrmode);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "screensaver", &flash8) == ESP_OK ) {
      cfg.screensaver = flash8;
      ESP_LOGI(TAG, "screensaver = %i", flash8);
    } else {
      ESP_LOGI(TAG, "screensaver set to default %i", cfg.screensaver);
      saveConfig();
    }

     if( nvs_get_i8(my_handle, "screenon", &flash8) == ESP_OK ) {
      cfg.screenon = flash8;
      ESP_LOGI(TAG, "screenon = %i", flash8);
    } else {
      ESP_LOGI(TAG, "screenon set to default %i", cfg.screenon);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "countermode", &flash8) == ESP_OK ) {
      cfg.countermode = flash8;
      ESP_LOGI(TAG, "countermode = %i", flash8);
    } else {
      ESP_LOGI(TAG, "countermode set to default %i", cfg.countermode);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "wifiscancycle", &flash8) == ESP_OK ) {
      cfg.wifiscancycle = flash8;
      ESP_LOGI(TAG, "wifiscancycle = %i", flash8);
    } else {
      ESP_LOGI(TAG, "WIFI scan cycle set to default %i", cfg.wifiscancycle);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "wifichancycle", &flash8) == ESP_OK ) {
      cfg.wifichancycle = flash8;
      ESP_LOGI(TAG, "wifichancycle = %i", flash8);
    } else {
      ESP_LOGI(TAG, "WIFI channel cycle set to default %i", cfg.wifichancycle);
      saveConfig();
    }
    
    if( nvs_get_i8(my_handle, "wifiant", &flash8) == ESP_OK ) {
      cfg.wifiant = flash8;
      ESP_LOGI(TAG, "wifiantenna = %i", flash8);
    } else {
      ESP_LOGI(TAG, "WIFI antenna switch set to default %i", cfg.wifiant);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "blescancycle", &flash8) == ESP_OK ) {
      cfg.blescancycle = flash8;
      ESP_LOGI(TAG, "blescancycle = %i", flash8);
    } else {
      ESP_LOGI(TAG, "BLEscan cycle set to default %i", cfg.blescancycle);
      saveConfig();
    }

    if( nvs_get_i8(my_handle, "blescanmode", &flash8) == ESP_OK ) {
      cfg.blescan = flash8;
      ESP_LOGI(TAG, "BLEscanmode = %i", flash8);
    } else {
      ESP_LOGI(TAG, "BLEscanmode set to default %i", cfg.blescan);
      saveConfig();
    }
    
    if( nvs_get_i16(my_handle, "rssilimit", &flash16) == ESP_OK ) {
      cfg.rssilimit = flash16;
      ESP_LOGI(TAG, "rssilimit = %i", flash16);
    } else {
      ESP_LOGI(TAG, "rssilimit set to default %i", cfg.rssilimit);
      saveConfig();
    }
    
    nvs_close(my_handle);
    ESP_LOGI(TAG, "Done");

    // put actions to be triggered on loaded config here
    #ifdef HAS_ANTENNA_SWITCH
      antenna_select(cfg.wifiant);
    #endif
    } 
}