#include "core/callbacks.h"
#include "esp_wifi.h"
#include "managers/gps_manager.h"
#include "managers/rgb_manager.h"
#include "managers/views/terminal_screen.h"
#include "managers/wifi_manager.h"
<<<<<<< HEAD
=======
#include "managers/status_display_manager.h"
#include "managers/ghostchi_manager.h"
#include "managers/ghostscript_runtime.h"
#include "core/utils.h"
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
#include "vendor/GPS/gps_logger.h"
#include "vendor/pcap.h"
#include <ctype.h>
#include <esp_log.h>
#include <string.h>
#include <time.h>
#include "esp_rom_sys.h"  // Contains esp_rom_printf

#define STORE_STR_ATTR __attribute__((section(".rodata.str")))
#define STORE_DATA_ATTR __attribute__((section(".rodata.data")))
#define WPS_OUI 0x0050f204
#define TAG "WIFI_MONITOR"
#define WPS_CONF_METHODS_PBC 0x0080
#define WPS_CONF_METHODS_PIN_DISPLAY 0x0004
#define WPS_CONF_METHODS_PIN_KEYPAD 0x0008
#define WIFI_PKT_DEAUTH 0x0C     // Deauth subtype
#define WIFI_PKT_BEACON 0x08     // Beacon subtype
#define WIFI_PKT_PROBE_REQ 0x04  // Probe Request subtype
#define WIFI_PKT_PROBE_RESP 0x05 // Probe Response subtype
#define WIFI_PKT_EAPOL 0x80
#define ESP_WIFI_VENDOR_METADATA_LEN 8 // Channel(1) + RSSI(1) + Rate(1) + Timestamp(4) + Noise(1)
#define MIN_SSIDS_FOR_DETECTION 2 // Minimum SSIDs needed to flag as PineAP
#define MAX_PINEAP_NETWORKS 20
#define MAX_SSIDS_PER_BSSID 10
#define MAX_WIFI_CHANNEL 13
#define CHANNEL_HOP_INTERVAL_MS 200
#define RECENT_SSID_COUNT 5
#define LOG_DELAY_MS 5000
static pineap_network_t pineap_networks[MAX_PINEAP_NETWORKS];
static int pineap_network_count = 0;
static bool pineap_detection_active = false;
static uint8_t current_channel = 1;
static esp_timer_handle_t channel_hop_timer = NULL;
static uint32_t hash_ssid(const char *ssid);
static bool ssid_hash_exists(pineap_network_t *network, uint32_t hash);
static void trim_trailing(char *str);
static bool compare_bssid(const uint8_t *bssid1, const uint8_t *bssid2);
static bool is_beacon_packet(const wifi_promiscuous_pkt_t *pkt);
<<<<<<< HEAD
static const char *SKIMMER_TAG STORE_STR_ATTR = "SKIMMER_DETECT";
=======
static pineap_network_t *find_or_create_network(const uint8_t *bssid);
#ifndef CONFIG_IDF_TARGET_ESP32S2
#endif

// handshake pairing and limited beacon emission for eapol capture
typedef struct {
    uint8_t ap[6];
    uint8_t sta[6];
    uint64_t replay;
    uint8_t ap_msg;   // 0=unknown, 1..4=M1..M4
    uint8_t sta_msg;  // 0=unknown, 1..4=M1..M4
} hs_entry_t;

#define HS_TABLE_MAX 16
static hs_entry_t hs_table[HS_TABLE_MAX];
static uint8_t hs_count_local = 0;
static uint8_t hs_insert_idx_local = 0;
static uint32_t hs_found_count = 0;
static portMUX_TYPE hs_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_pcap_enabled = true;

static inline bool mac_equal(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

uint32_t wifi_callbacks_get_handshake_count(void) {
    uint32_t count;
    portENTER_CRITICAL(&hs_mux);
    count = hs_found_count;
    portEXIT_CRITICAL(&hs_mux);
    return count;
}

void wifi_callbacks_reset_handshake_tracking(void) {
    portENTER_CRITICAL(&hs_mux);
    memset(hs_table, 0, sizeof(hs_table));
    hs_count_local = 0;
    hs_insert_idx_local = 0;
    hs_found_count = 0;
    portEXIT_CRITICAL(&hs_mux);
}

void wifi_callbacks_set_pcap_enabled(bool enabled) {
    s_pcap_enabled = enabled;
}

static const char *msg_name(uint8_t m) {
    switch (m) { case 1: return "M1"; case 2: return "M2"; case 3: return "M3"; case 4: return "M4"; default: return "M?"; }
}

static void process_eapol_candidate_pair(const uint8_t *ap,
                                         const uint8_t *sta,
                                         uint64_t replay,
                                         bool from_ap,
                                         uint8_t msg_type) {
    bool log_handshake = false;
    char log_ap_str[18];
    uint8_t log_ap_msg = 0;
    uint8_t log_sta_msg = 0;

    portENTER_CRITICAL(&hs_mux);
    for (uint8_t i = 0; i < hs_count_local; i++) {
        hs_entry_t *e = &hs_table[i];
        if (mac_equal(e->ap, ap) && mac_equal(e->sta, sta) && e->replay == replay) {
            if (from_ap) e->ap_msg = msg_type; else e->sta_msg = msg_type;
            if (e->ap_msg && e->sta_msg) {
                hs_found_count++;
                snprintf(log_ap_str, sizeof(log_ap_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                         e->ap[0], e->ap[1], e->ap[2], e->ap[3], e->ap[4], e->ap[5]);
                log_ap_msg = e->ap_msg;
                log_sta_msg = e->sta_msg;
                log_handshake = true;
                // reset to avoid duplicate notifications for same replay
                e->ap_msg = 0;
                e->sta_msg = 0;
            }
            portEXIT_CRITICAL(&hs_mux);
            if (log_handshake) {
                glog("Handshake found!\nAP=%s\nPair=%s/%s\n",
                     log_ap_str, msg_name(log_ap_msg), msg_name(log_sta_msg));
                char hs_payload[40];
                snprintf(hs_payload, sizeof(hs_payload), "%s|%s/%s",
                         log_ap_str, msg_name(log_ap_msg), msg_name(log_sta_msg));
                ghostscript_emit_event("handshake_captured", hs_payload);
            }
            return;
        }
    }
    uint8_t idx;
    if (hs_count_local < HS_TABLE_MAX) {
        idx = hs_count_local++;
    } else {
        idx = hs_insert_idx_local;
        hs_insert_idx_local = (hs_insert_idx_local + 1) % HS_TABLE_MAX;
    }
    hs_entry_t *ne = &hs_table[idx];
    memcpy(ne->ap, ap, 6);
    memcpy(ne->sta, sta, 6);
    ne->replay = replay;
    ne->ap_msg = from_ap ? msg_type : 0;
    ne->sta_msg = from_ap ? 0 : msg_type;
    portEXIT_CRITICAL(&hs_mux);
}

typedef struct {
    uint8_t bssid[6];
    uint8_t emitted;
    bool saw_nonempty_ssid;
} beacon_limiter_t;

#define BEACON_LIMIT_MAX 64
#define BEACON_MAX_PER_BSSID 3
static beacon_limiter_t beacon_limits[BEACON_LIMIT_MAX];
static uint8_t beacon_limit_count = 0;
static uint8_t beacon_limit_insert = 0;

// probe request dedupe to keep files small
#define PROBE_DEDUPE_MAX 64
typedef struct {
    uint8_t src[6];
    uint32_t ssid_hash;
    uint64_t last_ms;
} probe_dedupe_t;
static probe_dedupe_t probe_dedupe_tbl[PROBE_DEDUPE_MAX];
static uint8_t probe_dedupe_count = 0;
static uint8_t probe_dedupe_insert = 0;

static bool probe_should_emit(const uint8_t *src, uint32_t ssid_hash, uint64_t now_ms) {
    for (uint8_t i = 0; i < probe_dedupe_count; i++) {
        probe_dedupe_t *e = &probe_dedupe_tbl[i];
        if (memcmp(e->src, src, 6) == 0 && e->ssid_hash == ssid_hash) {
            if (now_ms - e->last_ms < PROBE_DEDUPE_TIMEOUT_MS) {
                return false;
            }
            e->last_ms = now_ms;
            return true;
        }
    }
    uint8_t idx;
    if (probe_dedupe_count < PROBE_DEDUPE_MAX) {
        idx = probe_dedupe_count++;
    } else {
        idx = probe_dedupe_insert;
        probe_dedupe_insert = (probe_dedupe_insert + 1) % PROBE_DEDUPE_MAX;
    }
    probe_dedupe_t *ne = &probe_dedupe_tbl[idx];
    memcpy(ne->src, src, 6);
    ne->ssid_hash = ssid_hash;
    ne->last_ms = now_ms;
    return true;
}

static bool beacon_should_emit_limited(const uint8_t *bssid, bool ssid_has_text) {
    for (uint8_t i = 0; i < beacon_limit_count; i++) {
        if (mac_equal(beacon_limits[i].bssid, bssid)) {
            if (beacon_limits[i].emitted >= BEACON_MAX_PER_BSSID) {
                if (!beacon_limits[i].saw_nonempty_ssid && ssid_has_text) {
                    beacon_limits[i].saw_nonempty_ssid = true;
                    return true;
                }
                return false;
            }
            beacon_limits[i].emitted++;
            if (ssid_has_text) beacon_limits[i].saw_nonempty_ssid = true;
            return true;
        }
    }
    uint8_t idx;
    if (beacon_limit_count < BEACON_LIMIT_MAX) {
        idx = beacon_limit_count++;
    } else {
        idx = beacon_limit_insert;
        beacon_limit_insert = (beacon_limit_insert + 1) % BEACON_LIMIT_MAX;
    }
    memcpy(beacon_limits[idx].bssid, bssid, 6);
    beacon_limits[idx].emitted = 1;
    beacon_limits[idx].saw_nonempty_ssid = ssid_has_text;
    return true;
}

// queued writer to avoid heavy work in promiscuous callback
typedef struct {
    uint16_t length;
    uint8_t data[768];
    bool in_use;
} pcap_pool_slot_t;

typedef struct {
    uint8_t slot_idx;
    pcap_capture_type_t cap_type;
} pcap_q_item_t;

#define EAPOL_Q_LEN 64
#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define PCAP_POOL_SLOTS_DEFAULT 10
#define PCAP_POOL_SLOTS_MIN 4
#else
#define PCAP_POOL_SLOTS_DEFAULT 16
#define PCAP_POOL_SLOTS_MIN 8
#endif
static QueueHandle_t s_pcap_q = NULL;
static TaskHandle_t s_pcap_writer_task = NULL;
static pcap_pool_slot_t *s_pcap_pool = NULL;
static size_t s_pcap_pool_slots = 0;
static portMUX_TYPE s_pcap_pool_lock = portMUX_INITIALIZER_UNLOCKED;

static bool pcap_pool_init(void) {
    if (s_pcap_pool != NULL && s_pcap_pool_slots > 0) {
        return true;
    }

    size_t slots = PCAP_POOL_SLOTS_DEFAULT;
    while (slots >= PCAP_POOL_SLOTS_MIN) {
        pcap_pool_slot_t *pool = (pcap_pool_slot_t *)heap_caps_calloc(slots, sizeof(pcap_pool_slot_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!pool) {
            pool = (pcap_pool_slot_t *)heap_caps_calloc(slots, sizeof(pcap_pool_slot_t), MALLOC_CAP_8BIT);
        }
        if (pool != NULL) {
            s_pcap_pool = pool;
            s_pcap_pool_slots = slots;
            ESP_LOGI(TAG, "PCAP pool allocated: %lu slots (%lu bytes)",
                     (unsigned long)s_pcap_pool_slots,
                     (unsigned long)(s_pcap_pool_slots * sizeof(pcap_pool_slot_t)));
            return true;
        }
        if (slots == PCAP_POOL_SLOTS_MIN) {
            break;
        }
        slots = (slots > 2) ? (slots - 2) : PCAP_POOL_SLOTS_MIN;
        if (slots < PCAP_POOL_SLOTS_MIN) {
            slots = PCAP_POOL_SLOTS_MIN;
        }
    }

    ESP_LOGE(TAG, "PCAP pool allocation failed");
    return false;
}

static int pcap_pool_acquire_slot(void) {
    if (s_pcap_pool == NULL || s_pcap_pool_slots == 0) {
        return -1;
    }

    int slot = -1;
    taskENTER_CRITICAL(&s_pcap_pool_lock);
    for (size_t i = 0; i < s_pcap_pool_slots; i++) {
        if (!s_pcap_pool[i].in_use) {
            s_pcap_pool[i].in_use = true;
            slot = (int)i;
            break;
        }
    }
    taskEXIT_CRITICAL(&s_pcap_pool_lock);
    return slot;
}

static void pcap_pool_release_slot(uint8_t slot_idx) {
    if (s_pcap_pool == NULL || s_pcap_pool_slots == 0 || slot_idx >= s_pcap_pool_slots) {
        return;
    }

    taskENTER_CRITICAL(&s_pcap_pool_lock);
    s_pcap_pool[slot_idx].in_use = false;
    s_pcap_pool[slot_idx].length = 0;
    taskEXIT_CRITICAL(&s_pcap_pool_lock);
}

static void pcap_writer_task(void *arg) {
    (void)arg;
    pcap_q_item_t item;
    uint32_t processed = 0;
    for (;;) {
        if (xQueueReceive(s_pcap_q, &item, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (s_pcap_pool != NULL && item.slot_idx < s_pcap_pool_slots) {
                pcap_pool_slot_t *slot = &s_pcap_pool[item.slot_idx];
                if (slot->length > 0) {
                    pcap_write_packet_to_buffer(slot->data, slot->length, item.cap_type);
                }
                pcap_pool_release_slot(item.slot_idx);
            }
            processed++;
            if ((processed & 0xFF) == 0) { // log occasionally to avoid spam
                UBaseType_t hwm_words = uxTaskGetStackHighWaterMark(NULL);
                glog("PCAP writer HWM (bytes): %lu\n", (unsigned long)hwm_words);
            }
            if ((processed & 0x1F) == 0 && pcap_auto_flush_enabled()) {
                pcap_flush_buffer_to_file();
            }
        } else {
            // periodic flush even if idle
            if (pcap_auto_flush_enabled()) {
                pcap_flush_buffer_to_file();
            }
        }
    }
}

static inline void ensure_pcap_queue_started(void) {
    if (s_pcap_q != NULL) {
        return;
    }

    if (!pcap_pool_init()) {
        return;
    }

    s_pcap_q = xQueueCreate(EAPOL_Q_LEN, sizeof(pcap_q_item_t));
    if (s_pcap_q != NULL && s_pcap_writer_task == NULL) {
        xTaskCreate(pcap_writer_task, "pcap_wr", 3072, NULL, 5, &s_pcap_writer_task);
    }
}

static inline void enqueue_pcap_write_typed(const uint8_t *payload, uint16_t len, pcap_capture_type_t cap_type) {
    if (!payload || len == 0) return;
    ensure_pcap_queue_started();
    if (!s_pcap_q) return;

    if (s_pcap_pool == NULL || s_pcap_pool_slots == 0) {
        return;
    }

    if (len > sizeof(s_pcap_pool[0].data)) {
        return;
    }

    int slot = pcap_pool_acquire_slot();
    if (slot < 0) {
        return;
    }

    pcap_pool_slot_t *pool_slot = &s_pcap_pool[slot];
    pool_slot->length = len;
    memcpy(pool_slot->data, payload, len);

    pcap_q_item_t item = {0};
    item.cap_type = cap_type;
    item.slot_idx = (uint8_t)slot;

    if (xQueueSend(s_pcap_q, &item, 0) != pdTRUE) {
        pcap_pool_release_slot((uint8_t)slot);
    }
}

static inline void enqueue_pcap_write(const uint8_t *payload, uint16_t len) {
    if (!s_pcap_enabled) return;
    enqueue_pcap_write_typed(payload, len, PCAP_CAPTURE_WIFI);
}

// cleanup function to free pcap queue and task when not capturing
void cleanup_pcap_queue(void) {
    if (s_pcap_writer_task != NULL) {
        vTaskDelete(s_pcap_writer_task);
        s_pcap_writer_task = NULL;
    }
    if (s_pcap_q != NULL) {
        // drain any remaining items and release pool slots
        pcap_q_item_t item;
        while (xQueueReceive(s_pcap_q, &item, 0) == pdTRUE) {
            pcap_pool_release_slot(item.slot_idx);
        }
        vQueueDelete(s_pcap_q);
        s_pcap_q = NULL;
    }

    if (s_pcap_pool != NULL) {
        pcap_pool_slot_t *pool_to_free = NULL;
        taskENTER_CRITICAL(&s_pcap_pool_lock);
        pool_to_free = s_pcap_pool;
        s_pcap_pool = NULL;
        s_pcap_pool_slots = 0;
        taskEXIT_CRITICAL(&s_pcap_pool_lock);
        heap_caps_free(pool_to_free);
    }
}

>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
static const char *suspicious_names[] STORE_DATA_ATTR = {
    "HC-03", "HC-05", "HC-06",  "HC-08",    "BT-HC05", "JDY-31",
    "AT-09", "HM-10", "CC41-A", "MLT-BT05", "SPP-CA",  "FFD0"};

wps_network_t detected_wps_networks[MAX_WPS_NETWORKS];
int detected_network_count = 0;
esp_timer_handle_t stop_timer;
int should_store_wps = 1;
gps_t *gps = NULL;
extern RGBManager_t rgb_manager;
typedef struct {
    uint8_t bssid[6];
    time_t detection_time;
    time_t last_update_time;
} blacklisted_ap_t;

static blacklisted_ap_t blacklist[MAX_PINEAP_NETWORKS];
static int blacklist_count = 0;

static bool is_blacklisted(const uint8_t *bssid) {
    for (int i = 0; i < blacklist_count; i++) {
        if (memcmp(blacklist[i].bssid, bssid, 6) == 0) {
            return true;
        }
    }
    return false;
}

static bool should_update_blacklisted(const uint8_t *bssid) {
    for (int i = 0; i < blacklist_count; i++) {
        if (memcmp(blacklist[i].bssid, bssid, 6) == 0) {
            time_t current_time = time(NULL);
            // Allow updates every 30 seconds
            if (current_time - blacklist[i].last_update_time >= 30) {
                blacklist[i].last_update_time = current_time;
                return true;
            }
            return false;
        }
    }
    return false;
}

static void add_to_blacklist(const uint8_t *bssid) {
    time_t current_time = time(NULL);

    // First check if BSSID exists
    for (int i = 0; i < blacklist_count; i++) {
        if (memcmp(blacklist[i].bssid, bssid, 6) == 0) {
            blacklist[i].last_update_time = current_time;
            return;
        }
    }

    // If not found and we have space, add new entry
    if (blacklist_count < MAX_PINEAP_NETWORKS) {
        memcpy(blacklist[blacklist_count].bssid, bssid, 6);
        blacklist[blacklist_count].detection_time = current_time;
        blacklist[blacklist_count].last_update_time = current_time;
        blacklist_count++;
    }
}


static void channel_hop_timer_callback(void *arg) {
    if (!pineap_detection_active)
        return;

    current_channel = (current_channel % MAX_WIFI_CHANNEL) + 1;
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
}

static esp_err_t start_channel_hopping(void) {
    esp_timer_create_args_t timer_args = {.callback = channel_hop_timer_callback,
                                          .name = "channel_hop"};

    if (channel_hop_timer == NULL) {
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &channel_hop_timer));
    }

    return esp_timer_start_periodic(channel_hop_timer, CHANNEL_HOP_INTERVAL_MS * 1000);
}

static void stop_channel_hopping(void) {
    if (channel_hop_timer) {
        esp_timer_stop(channel_hop_timer);
        esp_timer_delete(channel_hop_timer);
        channel_hop_timer = NULL;
    }
}

static pineap_network_t *find_or_create_network(const uint8_t *bssid) {
    for (int i = 0; i < pineap_network_count; i++) {
        if (compare_bssid(pineap_networks[i].bssid, bssid)) {
            return &pineap_networks[i];
        }
    }

    // If not found and we have space, create new entry
    if (pineap_network_count < MAX_PINEAP_NETWORKS) {
        pineap_network_t *network = &pineap_networks[pineap_network_count++];
        memcpy(network->bssid, bssid, 6);
        network->ssid_count = 0;
        network->is_pineap = false;
        network->first_seen = time(NULL);
        return network;
    }

    return NULL;
}

static uint32_t hash_ssid(const char *ssid) {
    uint32_t hash = 5381;
    int c;
    while ((c = *ssid++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}

static bool ssid_hash_exists(pineap_network_t *network, uint32_t hash) {
    for (int i = 0; i < network->ssid_count; i++) {
        if (network->ssid_hashes[i] == hash) {
            return true;
        }
    }
    return false;
}

<<<<<<< HEAD
=======
static void wardrive_send_probe_request(void) {
    // Broadcast probe request frame
    uint8_t probe_req[] = {
        0x40, 0x00,                         // Frame Control: Probe Request
        0x00, 0x00,                         // Duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination: broadcast
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source: filled below
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // BSSID: broadcast
        0x00, 0x00,                         // Sequence Control
        // SSID IE (wildcard - empty means "any SSID")
        0x00, 0x00,
        // Supported Rates IE
        0x01, 0x08, 0x02, 0x04, 0x0b, 0x16, 0x0c, 0x12, 0x18, 0x24,
        // Extended Supported Rates IE
        0x32, 0x04, 0x30, 0x48, 0x60, 0x6c,
        // DS Parameter Set (current channel)
        0x03, 0x01, 0x01  // Channel placeholder
    };
    
    // Get our MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    memcpy(&probe_req[10], mac, 6);
    
    // Set current channel in DS Parameter Set
    probe_req[sizeof(probe_req) - 1] = wardrive_channel;
    
    esp_wifi_80211_tx(WIFI_IF_STA, probe_req, sizeof(probe_req), false);
}

static int hop_count = 0;

static void wardrive_hop_timer_callback(void *arg) {
    if (!wardriving_hopping_active)
        return;

    if (wardrive_role == WARDRIVE_ROLE_PRIMARY && wardrive_peer_assist_active && !esp_comm_manager_is_connected()) {
        wardrive_peer_assist_active = false;
        gps_manager_set_peer_gps_preferred(false);
        gps_manager_clear_peer_fix();
        wardrive_build_channel_list();
        wardrive_channel_idx = 0;
        if (wardrive_channel_count > 0) {
            wardrive_channel = wardrive_channels[0];
            (void)esp_wifi_set_channel(wardrive_channel, WIFI_SECOND_CHAN_NONE);
        }
        glog("Wardrive: peer helper link lost, continuing local scan only\n");
        wardrive_apply_hop_interval();
    }

    if (wardrive_channel_count == 0) return;
    wardrive_channel_idx = (wardrive_channel_idx + 1) % wardrive_channel_count;
    wardrive_channel = wardrive_channels[wardrive_channel_idx];
    esp_wifi_set_channel(wardrive_channel, WIFI_SECOND_CHAN_NONE);
    
    // Send probe request to trigger AP responses
    wardrive_send_probe_request();
    
    hop_count++;
    if (hop_count % 200 == 0) {
        ESP_LOGI(TAG, "Wardrive hopped to channel %d (hop #%d)", wardrive_channel, hop_count);
    }
}

static esp_err_t start_wardrive_channel_hopping(void) {
    esp_timer_create_args_t timer_args = {.callback = wardrive_hop_timer_callback,
                                          .name = "wardrive_hop"};

    if (wardrive_hop_timer == NULL) {
        esp_err_t err = esp_timer_create(&timer_args, &wardrive_hop_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create wardrive hop timer: %s", esp_err_to_name(err));
            return err;
        }
    }

    wardrive_build_channel_list();
    wardrive_channel_idx = 0;
    wardrive_channel = wardrive_channels[0];
    wardriving_hopping_active = true;
    hop_count = 0;
    
    esp_err_t err = esp_wifi_set_channel(wardrive_channel, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG, "Wardrive starting on channel %d (set_channel: %s)", wardrive_channel, esp_err_to_name(err));
    
    uint32_t interval_ms = wardrive_get_hop_interval_ms();
    err = esp_timer_start_periodic(wardrive_hop_timer, (uint64_t)interval_ms * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wardrive hop timer: %s", esp_err_to_name(err));
        toast_show("Wardrive hop failed", TOAST_ERROR);
        return err;
    }
    ESP_LOGI(TAG,
             "Wardrive channel hopping started (%d channels, %lums interval, role=%s)",
             wardrive_channel_count,
             (unsigned long)interval_ms,
             wardrive_role == WARDRIVE_ROLE_PRIMARY ? "primary" : "helper");
    return ESP_OK;
}

static void stop_wardrive_channel_hopping(void) {
    wardriving_hopping_active = false;
    if (wardrive_hop_timer) {
        esp_timer_stop(wardrive_hop_timer);
        esp_timer_delete(wardrive_hop_timer);
        wardrive_hop_timer = NULL;
    }
}

#define WARDRIVE_HEARTBEAT_INTERVAL_MS 10000

static void wardrive_heartbeat_cb(void *arg) {
    (void)arg;

    if (!wardriving_hopping_active) {
        return;
    }

    gps_t gps_snapshot = {0};
    bool using_peer_gps = false;
    bool have_active_gps = gps_manager_get_active_gps_snapshot(&gps_snapshot, &using_peer_gps);
    bool peer_preferred = gps_manager_is_peer_gps_preferred();
    gps_t *gps_local = NULL;
    const char *fix_status = "No GPS";
    char fix_status_buf[24] = {0};
    uint8_t sats = 0;

    if (have_active_gps) {
        gps_local = &gps_snapshot;
    } else if (!peer_preferred) {
        static gps_t local_snapshot = {0};
        if (gps_manager_get_local_gps_snapshot(&local_snapshot)) {
            gps_local = &local_snapshot;
        }
    }

    if (gps_local != NULL) {
        sats = gps_local->sats_in_use;
        if (!gps_local->valid || gps_local->fix < GPS_FIX_GPS || gps_local->fix_mode < GPS_MODE_2D) {
            fix_status = using_peer_gps ? "Peer No Fix" : "No Fix";
        } else if (gps_local->fix_mode == GPS_MODE_2D) {
            fix_status = using_peer_gps ? "Peer 2D" : "2D";
        } else if (gps_local->fix_mode == GPS_MODE_3D) {
            fix_status = using_peer_gps ? "Peer 3D" : "3D";
        } else {
            fix_status = using_peer_gps ? "Peer Fix" : "Fix";
        }

        if (!wardrive_is_valid_date(&gps_local->date)) {
            snprintf(fix_status_buf, sizeof(fix_status_buf), "%s/NoDate", fix_status);
            fix_status = fix_status_buf;
        }
    } else if (peer_preferred) {
        fix_status = "Peer Stale";
    }

    uint32_t up_s = 0;
    if (wardrive_start_us != 0) {
        up_s = (uint32_t)((esp_timer_get_time() - wardrive_start_us) / 1000000LL);
    }

    uint32_t up_m = up_s / 60;
    uint32_t up_rem_s = up_s % 60;

    size_t pending = csv_get_pending_bytes();
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    if (wardrive_role == WARDRIVE_ROLE_HELPER) {
        glog("Wardrive: ap=%lu logged=%lu/%lu gpsrej=%lu helper=%lu/%lu tx(n/p/r/t/s)=%lu/%lu/%lu/%lu/%lu send(ok/fail)=%lu/%lu peergps(rx/fix tx_ok/fail)=%lu/%lu %lu/%lu ch=%u up=%lum%02lus gps=%s/%u pending=%uB heap=%u/%uB\n",
             (unsigned long)wardrive_wifi_frames_seen,
             (unsigned long)wardrive_log_ok,
             (unsigned long)wardrive_log_attempts,
             (unsigned long)wardrive_gps_rejected,
             (unsigned long)wardrive_helper_merged_ok,
             (unsigned long)wardrive_helper_rx_observations,
             (unsigned long)wardrive_helper_tx_new,
             (unsigned long)wardrive_helper_tx_ssid_promo,
             (unsigned long)wardrive_helper_tx_rssi,
             (unsigned long)wardrive_helper_tx_refresh,
             (unsigned long)wardrive_helper_tx_suppressed,
             (unsigned long)wardrive_helper_stream_send_ok,
             (unsigned long)wardrive_helper_stream_send_fail,
             (unsigned long)peer_gps_stream_rx_packets,
             (unsigned long)peer_gps_stream_rx_fix_packets,
             (unsigned long)peer_gps_stream_tx_ok,
             (unsigned long)peer_gps_stream_tx_fail,
             (unsigned)wardrive_channel,
             (unsigned long)up_m,
             (unsigned long)up_rem_s,
             fix_status,
             (unsigned)sats,
             (unsigned)pending,
             (unsigned)heap_free,
             (unsigned)heap_largest);
    } else {
        glog("Wardrive: ap=%lu logged=%lu/%lu gpsrej=%lu helper=%lu/%lu peergps(rx/fix tx_ok/fail)=%lu/%lu %lu/%lu ch=%u up=%lum%02lus gps=%s/%u pending=%uB heap=%u/%uB\n",
             (unsigned long)wardrive_wifi_frames_seen,
             (unsigned long)wardrive_log_ok,
             (unsigned long)wardrive_log_attempts,
             (unsigned long)wardrive_gps_rejected,
             (unsigned long)wardrive_helper_merged_ok,
             (unsigned long)wardrive_helper_rx_observations,
             (unsigned long)peer_gps_stream_rx_packets,
             (unsigned long)peer_gps_stream_rx_fix_packets,
             (unsigned long)peer_gps_stream_tx_ok,
             (unsigned long)peer_gps_stream_tx_fail,
             (unsigned)wardrive_channel,
             (unsigned long)up_m,
             (unsigned long)up_rem_s,
             fix_status,
             (unsigned)sats,
             (unsigned)pending,
             (unsigned)heap_free,
             (unsigned)heap_largest);
    }
}

static void start_wardrive_heartbeat(void) {
    wardrive_start_us = esp_timer_get_time();
    wardrive_wifi_frames_seen = 0;
    wardrive_ble_advs_seen = 0;
    wardrive_log_attempts = 0;
    wardrive_log_ok = 0;
    wardrive_gps_rejected = 0;
    wardrive_helper_rx_observations = 0;
    wardrive_helper_merged_ok = 0;
    wardrive_helper_tx_new = 0;
    wardrive_helper_tx_ssid_promo = 0;
    wardrive_helper_tx_rssi = 0;
    wardrive_helper_tx_refresh = 0;
    wardrive_helper_tx_suppressed = 0;
    wardrive_helper_stream_send_ok = 0;
    wardrive_helper_stream_send_fail = 0;
    peer_gps_stream_tx_ok = 0;
    peer_gps_stream_tx_fail = 0;
    peer_gps_stream_rx_packets = 0;
    peer_gps_stream_rx_fix_packets = 0;
    memset(wardrive_helper_dedupe, 0, sizeof(wardrive_helper_dedupe));
    wardrive_helper_dedupe_idx = 0;
#ifndef CONFIG_IDF_TARGET_ESP32S2
    ble_wardriving_reset_unique_device_count();
#endif

    if (!wardrive_heartbeat_timer) {
        const esp_timer_create_args_t timer_args = {
            .callback = &wardrive_heartbeat_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "wardrive_hb"
        };
        esp_err_t err = esp_timer_create(&timer_args, &wardrive_heartbeat_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create wardrive heartbeat timer: %s", esp_err_to_name(err));
            return;
        }
    }

    (void)esp_timer_stop(wardrive_heartbeat_timer);
    esp_err_t start_err = esp_timer_start_periodic(wardrive_heartbeat_timer,
                                                    (uint64_t)WARDRIVE_HEARTBEAT_INTERVAL_MS * 1000ULL);
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wardrive heartbeat timer: %s", esp_err_to_name(start_err));
    }
}

static void stop_wardrive_heartbeat(void) {
    if (wardrive_heartbeat_timer) {
        esp_timer_stop(wardrive_heartbeat_timer);
        esp_timer_delete(wardrive_heartbeat_timer);
        wardrive_heartbeat_timer = NULL;
    }
}

static void pineap_log_worker_task(void *arg) {
    (void)arg;
    pineap_log_event_t ev;

    for (;;) {
        if (xQueueReceive(s_pineap_log_queue, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        uint32_t now = now_ms_u32();
        if (ev.due_ms > now) {
            vTaskDelay(pdMS_TO_TICKS(ev.due_ms - now));
        }

        if (!pineap_detection_active || pineap_networks == NULL) {
            continue;
        }

        if (ev.network_index >= (uint8_t)pineap_network_count) {
            continue;
        }

        pineap_network_t *network = &pineap_networks[ev.network_index];
        if (!network->log_pending || network->log_due_ms != ev.due_ms) {
            continue;
        }

        char mac_str[18];
        format_mac_address(network->bssid, mac_str, sizeof(mac_str), false);

        char ssids_str[256] = {0};
        int valid_ssid_count = build_recent_ssids_string(network, ssids_str, sizeof(ssids_str));

        if (valid_ssid_count >= MIN_SSIDS_FOR_DETECTION) {
            pulse_once(&rgb_manager, 255, 0, 255);

            for (int i = 0; i < pineap_network_count; i++) {
                if (i != (network - pineap_networks) &&
                    strcasecmp(network->recent_ssids[0], pineap_networks[i].recent_ssids[0]) == 0) {
                    char other_mac_str[18];
                    format_mac_address(pineap_networks[i].bssid, other_mac_str, sizeof(other_mac_str), false);
                    glog("Evil Twin Detected:\nSame SSID '%.100s'\nfrom BSSID %s and\n%s\n",
                         network->recent_ssids[0], mac_str, other_mac_str);
                }
            }

            log_pineap_details(network, "Pineapple detected!", ssids_str, valid_ssid_count);
        }

        network->log_pending = false;
        network->log_due_ms = 0;
    }
}

static void start_pineap_log_worker(void) {
    if (s_pineap_log_queue == NULL) {
        s_pineap_log_queue = xQueueCreate(PINEAP_LOG_QUEUE_LEN, sizeof(pineap_log_event_t));
    }
    if (s_pineap_log_queue != NULL && s_pineap_log_task == NULL) {
        if (xTaskCreate(pineap_log_worker_task, "pineap_logw", 1792, NULL, 1, &s_pineap_log_task) != pdPASS) {
            s_pineap_log_task = NULL;
        }
    }
}

static void stop_pineap_log_worker(void) {
    if (s_pineap_log_task != NULL) {
        vTaskDelete(s_pineap_log_task);
        s_pineap_log_task = NULL;
    }
    if (s_pineap_log_queue != NULL) {
        vQueueDelete(s_pineap_log_queue);
        s_pineap_log_queue = NULL;
    }
}

>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
void start_pineap_detection(void) {
    pineap_detection_active = true;
    pineap_network_count = 0;
    memset(pineap_networks, 0, sizeof(pineap_networks));
    current_channel = 1;
    start_channel_hopping();
}

void stop_pineap_detection(void) {
    pineap_detection_active = false;
    stop_channel_hopping();
}

#define IRAM_PRINTF(fmt, ...) do { \
    static const char flash_fmt[] STORE_STR_ATTR = fmt; \
    esp_rom_printf(flash_fmt, ##__VA_ARGS__); \
} while(0)

void log_pineap_detection(void *arg) {
    pineap_log_data_t *log_data = (pineap_log_data_t *)arg;
    pineap_network_t *network = log_data->network;

    vTaskDelay(pdMS_TO_TICKS(5000));

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", log_data->bssid[0],
             log_data->bssid[1], log_data->bssid[2], log_data->bssid[3], log_data->bssid[4],
             log_data->bssid[5]);

    // Build SSIDs string, filtering out empty SSIDs
    char ssids_str[256] = {0};
    int valid_ssid_count = 0;

    // Use the most up-to-date SSIDs from the network structure
    for (int i = 0; i < network->ssid_count && i < RECENT_SSID_COUNT; i++) {
        if (strlen(network->recent_ssids[i]) > 0) {
            if (valid_ssid_count > 0)
                strcat(ssids_str, ", ");
            strcat(ssids_str, network->recent_ssids[i]);
            valid_ssid_count++;
        }
    }

    // Only log if we have valid SSIDs
    if (valid_ssid_count >= MIN_SSIDS_FOR_DETECTION) {
        // Pulse RGB purple (red + blue) to indicate Pineapple detection
        pulse_once(&rgb_manager, 255, 0, 255);

        IRAM_PRINTF("\nPineapple detected!\nBSSID: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                   log_data->bssid[0], log_data->bssid[1], log_data->bssid[2],
                   log_data->bssid[3], log_data->bssid[4], log_data->bssid[5]);
        IRAM_PRINTF("Channel: %d\n", network->last_channel);
        IRAM_PRINTF("RSSI: %d\n", network->last_rssi);
        IRAM_PRINTF("SSIDs (%d): %s\n", valid_ssid_count, ssids_str);

        // Evil Twin Detection: Check for same SSID from different BSSIDs
        for (int i = 0; i < pineap_network_count; i++) {
            if (i != (network - pineap_networks) && // Skip self
                strcasecmp(network->recent_ssids[0], pineap_networks[i].recent_ssids[0]) == 0) {
                IRAM_PRINTF("Evil Twin:\nSSID '%.100s'\nBSSID %.17s vs %.100s\n",
                           network->recent_ssids[0], mac_str, pineap_networks[i].bssid);
                TERMINAL_VIEW_ADD_TEXT(
                    "Evil Twin Detected:\nSame SSID '%.100s'\nfrom BSSID %.17s and\n%.100s\n",
                    network->recent_ssids[0], mac_str, pineap_networks[i].bssid);
            }
        }

        TERMINAL_VIEW_ADD_TEXT("\nPineapple detected!\n");
        TERMINAL_VIEW_ADD_TEXT("BSSID: %s\n", mac_str);
        TERMINAL_VIEW_ADD_TEXT("Channel: %d\n", network->last_channel);
        TERMINAL_VIEW_ADD_TEXT("RSSI: %d\n", network->last_rssi);
        TERMINAL_VIEW_ADD_TEXT("SSIDs (%d): %s\n", valid_ssid_count, ssids_str);
    }

    free(log_data);
    network->log_task_handle = NULL; // Clear handle before deletion
    vTaskDelete(NULL);
}

static void start_log_task(pineap_network_t *network, const char *new_ssid, int8_t channel,
                           int8_t rssi) {
    // Check if a task is already running
    if (network->log_task_handle != NULL) {
        TaskHandle_t existing_handle = network->log_task_handle;
        network->log_task_handle = NULL; // Clear it first to avoid race conditions
        vTaskDelete(existing_handle);    // Clean up existing task
    }

    pineap_log_data_t *log_data = malloc(sizeof(pineap_log_data_t));
    if (!log_data)
        return;

    // Copy network data
    memcpy(log_data->bssid, network->bssid, 6);
    memcpy(log_data->recent_ssids, network->recent_ssids, sizeof(network->recent_ssids));
    log_data->ssid_count = network->ssid_count;
    log_data->channel = channel;
    log_data->rssi = rssi;
    log_data->network = network;
    BaseType_t result = xTaskCreate(log_pineap_detection, "pineap_log", 4096, log_data, 1,
                                    &network->log_task_handle);
    if (result != pdPASS) {
        free(log_data);
        network->log_task_handle = NULL;
    }
}

// Helper function to check if SSID is valid and unique
static bool is_valid_unique_ssid(const char *new_ssid, pineap_network_t *network) {
    // Check if SSID is empty or just whitespace
    if (strlen(new_ssid) == 0)
        return false;

    bool all_whitespace = true;
    for (const char *p = new_ssid; *p; p++) {
        if (!isspace((unsigned char)*p)) {
            all_whitespace = false;
            break;
        }
    }
    if (all_whitespace)
        return false;

    // Check if this SSID is already in our recent list
    for (int i = 0; i < network->ssid_count && i < RECENT_SSID_COUNT; i++) {
        if (strcasecmp(network->recent_ssids[i], new_ssid) == 0) {
            return false; // SSID already exists (case insensitive)
        }
    }

    return true;
}

void wifi_pineap_detector_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!pineap_detection_active || type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    // Only process beacon frames
    if (!is_beacon_packet(ppkt))
        return;

    // Find or create network
    pineap_network_t *network = find_or_create_network(hdr->addr3);
    if (!network)
        return;

    // Update channel and RSSI
    network->last_channel = ppkt->rx_ctrl.channel;
    network->last_rssi = ppkt->rx_ctrl.rssi;

    // Extract SSID from beacon
    const uint8_t *payload = ppkt->payload;
    int len = ppkt->rx_ctrl.sig_len;

    // Skip fixed parameters (24 bytes header + 12 bytes fixed params)
    int index = 36;
    if (index + 2 > len)
        return;

    // Look specifically for SSID element (ID = 0)
    if (payload[index] != 0)
        return;

    uint8_t ie_len = payload[index + 1];
    if (ie_len > 32 || index + 2 + ie_len > len)
        return;

    // Get SSID
    char ssid[33] = {0};
    memcpy(ssid, &payload[index + 2], ie_len);
    ssid[ie_len] = '\0';
    trim_trailing(ssid);

    // Only proceed if this is a valid and unique SSID
    if (!is_valid_unique_ssid(ssid, network))
        return;

    uint32_t ssid_hash = hash_ssid(ssid);

    // If this is a new SSID hash for this BSSID, add it
    if (!ssid_hash_exists(network, ssid_hash) && network->ssid_count < MAX_SSIDS_PER_BSSID) {
        network->ssid_hashes[network->ssid_count++] = ssid_hash;

        // Add to recent SSIDs circular buffer
        strncpy(network->recent_ssids[network->recent_ssid_index], ssid, 32);
        network->recent_ssid_index = (network->recent_ssid_index + 1) % RECENT_SSID_COUNT;

        // If we detect multiple SSIDs from same BSSID, mark as potential Pineap
        if (network->ssid_count >= MIN_SSIDS_FOR_DETECTION &&
            (!is_blacklisted(hdr->addr3) || should_update_blacklisted(hdr->addr3))) {

            network->is_pineap = true;
            add_to_blacklist(hdr->addr3);

            // Create new logging task if previous one has completed
            if (network->log_task_handle == NULL) {
                pineap_log_data_t *log_data = malloc(sizeof(pineap_log_data_t));
                if (!log_data)
                    return;

                memcpy(log_data->bssid, network->bssid, 6);
                log_data->network = network; // Pass network pointer for up-to-date info

                BaseType_t result = xTaskCreate(log_pineap_detection, "pineap_log", 4096, log_data,
                                                1, &network->log_task_handle);
                if (result != pdPASS) {
                    free(log_data);
                    network->log_task_handle = NULL;
                }
            }

            // Write to PCAP if capture is active
            if (pcap_file != NULL) {
                pcap_write_packet_to_buffer(ppkt->payload, ppkt->rx_ctrl.sig_len,
                                            PCAP_CAPTURE_WIFI);
            }
        }
    }
}

static void trim_trailing(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r')) {
        str[i] = '\0';
        i--;
    }
}

void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                       void *event_data) {
    switch (event_id) {
    case GPS_UPDATE:
        gps = (gps_t *)event_data;
        break;
    default:
        break;
    }
}

bool compare_bssid(const uint8_t *bssid1, const uint8_t *bssid2) {
    for (int i = 0; i < 6; i++) {
        if (bssid1[i] != bssid2[i]) {
            return false;
        }
    }
    return true;
}

bool is_network_duplicate(const char *ssid, const uint8_t *bssid) {
    for (int i = 0; i < detected_network_count; i++) {
        if (strcmp(detected_wps_networks[i].ssid, ssid) == 0 &&
            compare_bssid(detected_wps_networks[i].bssid, bssid)) {
            return true;
        }
    }
    return false;
}

void get_frame_type_and_subtype(const wifi_promiscuous_pkt_t *pkt, uint8_t *frame_type,
                                uint8_t *frame_subtype) {
    if (pkt->rx_ctrl.sig_len < 24) {
        *frame_type = 0xFF;
        *frame_subtype = 0xFF;
        return;
    }

    const uint8_t *frame_ctrl = pkt->payload;

    *frame_type = (frame_ctrl[0] & 0x0C) >> 2;
    *frame_subtype = (frame_ctrl[0] & 0xF0) >> 4;
}

bool is_beacon_packet(const wifi_promiscuous_pkt_t *pkt) {
    uint8_t frame_type, frame_subtype;
    get_frame_type_and_subtype(pkt, &frame_type, &frame_subtype);
    return (frame_type == WIFI_PKT_MGMT && frame_subtype == WIFI_PKT_BEACON);
}

bool is_deauth_packet(const wifi_promiscuous_pkt_t *pkt) {
    uint8_t frame_type, frame_subtype;
    get_frame_type_and_subtype(pkt, &frame_type, &frame_subtype);
    return (frame_type == WIFI_PKT_MGMT && frame_subtype == WIFI_PKT_DEAUTH);
}

bool is_probe_request(const wifi_promiscuous_pkt_t *pkt) {
    uint8_t frame_type, frame_subtype;
    get_frame_type_and_subtype(pkt, &frame_type, &frame_subtype);
    return (frame_type == WIFI_PKT_MGMT && frame_subtype == WIFI_PKT_PROBE_REQ);
}

bool is_probe_response(const wifi_promiscuous_pkt_t *pkt) {
    uint8_t frame_type, frame_subtype;
    get_frame_type_and_subtype(pkt, &frame_type, &frame_subtype);
    return (frame_type == WIFI_PKT_MGMT && frame_subtype == WIFI_PKT_PROBE_RESP);
}

bool is_eapol_response(const wifi_promiscuous_pkt_t *pkt) {
    const uint8_t *frame = pkt->payload;

    if ((frame[30] == 0x88 && frame[31] == 0x8E) || (frame[32] == 0x88 && frame[33] == 0x8E)) {
        return true;
    }

    return false;
}

bool is_pwn_response(const wifi_promiscuous_pkt_t *pkt) {
    const uint8_t *frame = pkt->payload;

    if (frame[0] == 0x80) {
        return true;
    }

    return false;
}

void wifi_raw_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("RAW_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wardriving_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)pkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    const uint8_t *payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    uint8_t frame_type = hdr->frame_ctrl & 0xFC;
    if (frame_type != 0x80 && frame_type != 0x50) {
        return;
    }

    int index = 36;
    char ssid[33] = {0};
    uint8_t bssid[6];
    memcpy(bssid, hdr->addr3, 6);

    int rssi = pkt->rx_ctrl.rssi;
    int channel = pkt->rx_ctrl.channel;

    bool network_found = false;
    char encryption_type[8] = "OPEN";

    while (index + 1 < len) {
        uint8_t id = payload[index];
        uint8_t ie_len = payload[index + 1];

        if (index + 2 + ie_len > len) {
            break;
        }

        if (id == 0 && ie_len <= 32) {
            memcpy(ssid, &payload[index + 2], ie_len);
            ssid[ie_len] = '\0';
            trim_trailing(ssid);
        }

        if (id == 48) {
            strncpy(encryption_type, "WPA2", sizeof(encryption_type));
        } else if (id == 221) {
            uint32_t oui =
                (payload[index + 2] << 16) | (payload[index + 3] << 8) | payload[index + 4];
            uint8_t oui_type = payload[index + 5];
            if (oui == 0x0050f2 && oui_type == 0x01) {
                strncpy(encryption_type, "WPA", sizeof(encryption_type));
            } else if (oui == 0x0050f2 && oui_type == 0x02) {
                strncpy(encryption_type, "WEP", sizeof(encryption_type));
            }
        }

        index += (2 + ie_len);
    }

    double latitude = 0;
    double longitude = 0;

    if (gps != NULL) {
        latitude = gps->latitude;
        longitude = gps->longitude;
    }

    wardriving_data_t wardriving_data;
    strncpy(wardriving_data.ssid, ssid, sizeof(wardriving_data.ssid) - 1);
    wardriving_data.ssid[sizeof(wardriving_data.ssid) - 1] = '\0'; // Null-terminate
    snprintf(wardriving_data.bssid, sizeof(wardriving_data.bssid), "%02x:%02x:%02x:%02x:%02x:%02x",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    wardriving_data.rssi = rssi;
    wardriving_data.channel = channel;
    wardriving_data.latitude = latitude;
    wardriving_data.longitude = longitude;
    strncpy(wardriving_data.encryption_type, encryption_type,
            sizeof(wardriving_data.encryption_type) - 1);
    wardriving_data.encryption_type[sizeof(wardriving_data.encryption_type) - 1] = '\0';

    esp_err_t err = gps_manager_log_wardriving_data(&wardriving_data);
}

void wifi_probe_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("PROBE_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wifi_beacon_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("BEACON_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wifi_deauth_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("DEAUTH_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wifi_pwn_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("PWN_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wifi_eapol_scan_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT)
        return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len > 0) {
        esp_err_t ret =
            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len, PCAP_CAPTURE_WIFI);
        if (ret != ESP_OK) {
            ESP_LOGE("EAPOL_SCAN", "Failed to write packet to buffer");
        }
    }
}

void wifi_wps_detection_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)pkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    const uint8_t *payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    uint8_t frame_type = hdr->frame_ctrl & 0xFC;
    if (frame_type != 0x80 && frame_type != 0x50) {
        return;
    }

    int index = 36;
    char ssid[33] = {0};
    bool wps_found = false;
    uint8_t bssid[6];
    memcpy(bssid, hdr->addr3, 6);

    while (index + 1 < len) {
        uint8_t id = payload[index];
        uint8_t ie_len = payload[index + 1];

        if (index + 2 + ie_len > len) {
            break;
        }

        if (id == 0 && ie_len <= 32) {
            memcpy(ssid, &payload[index + 2], ie_len);
            ssid[ie_len] = '\0';
            trim_trailing(ssid);
        }

        if (is_network_duplicate(ssid, bssid)) {
            return;
        }

        if (id == 221 && ie_len >= 4) {
            uint32_t oui =
                (payload[index + 2] << 16) | (payload[index + 3] << 8) | payload[index + 4];
            uint8_t oui_type = payload[index + 5];

            if (oui == 0x0050f2 && oui_type == 0x04) {
                wps_found = true;

                int attr_index = index + 6;
                int wps_ie_end = index + 2 + ie_len;

                while (attr_index + 4 <= wps_ie_end) {
                    uint16_t attr_id = (payload[attr_index] << 8) | payload[attr_index + 1];
                    uint16_t attr_len = (payload[attr_index + 2] << 8) | payload[attr_index + 3];

                    if (attr_len > (wps_ie_end - (attr_index + 4))) {
                        break;
                    }

                    if (attr_id == 0x1008 && attr_len == 2) {
                        uint16_t config_methods =
                            (payload[attr_index + 4] << 8) | payload[attr_index + 5];

                        IRAM_PRINTF("Configuration Methods found: 0x%04x\n", config_methods);

                        if (config_methods & WPS_CONF_METHODS_PBC) {
                            IRAM_PRINTF("WPS Push Button detected:\n%s\n", ssid);
                            TERMINAL_VIEW_ADD_TEXT("WPS Push Button detected:\n%s\n", ssid);
                        } else if (config_methods &
                                   (WPS_CONF_METHODS_PIN_DISPLAY | WPS_CONF_METHODS_PIN_KEYPAD)) {
                            IRAM_PRINTF("WPS PIN detected:\n%s\n", ssid);
                            TERMINAL_VIEW_ADD_TEXT("WPS PIN detected:\n%s\n", ssid);
                        }

                        if (should_store_wps == 1) {
                            wps_network_t new_network;
                            strncpy(new_network.ssid, ssid, sizeof(new_network.ssid) - 1);
                            new_network.ssid[sizeof(new_network.ssid) - 1] =
                                '\0'; // Ensure null termination
                            memcpy(new_network.bssid, bssid, sizeof(new_network.bssid));
                            new_network.wps_enabled = true;
                            new_network.wps_mode = config_methods & (WPS_CONF_METHODS_PIN_DISPLAY |
                                                                     WPS_CONF_METHODS_PIN_KEYPAD)
                                                       ? WPS_MODE_PIN
                                                       : WPS_MODE_PBC;

                            detected_wps_networks[detected_network_count++] = new_network;
                        } else {
                            pcap_write_packet_to_buffer(pkt->payload, pkt->rx_ctrl.sig_len,
                                                        PCAP_CAPTURE_WIFI);
                        }

                        if (detected_network_count >= MAX_WPS_NETWORKS) {
                            IRAM_PRINTF("Maximum number of WPS networks detected\nStopping monitor "
                                   "mode.\n");
                            TERMINAL_VIEW_ADD_TEXT(
                                "Maximum number of WPS networks detected\nStopping "
                                "monitor mode.\n");
                            wifi_manager_stop_monitor_mode();
                        }
                    }

                    attr_index += (4 + attr_len);
                }
            }
        }

        index += (2 + ie_len);
    }
}

#ifndef CONFIG_IDF_TARGET_ESP32S2
// Forward declare the struct and callback before use
struct ble_hs_adv_field;
static int ble_hs_adv_parse_fields_cb(const struct ble_hs_adv_field *field, void *arg);

void ble_wardriving_callback(struct ble_gap_event *event, void *arg) {
    if (!event || event->type != BLE_GAP_EVENT_DISC) {
        return;
    }

    wardriving_data_t wardriving_data = {0};
    wardriving_data.ble_data.is_ble_device = true;

    // Get BLE MAC and RSSI
    snprintf(wardriving_data.ble_data.ble_mac, sizeof(wardriving_data.ble_data.ble_mac),
             "%02x:%02x:%02x:%02x:%02x:%02x", event->disc.addr.val[0], event->disc.addr.val[1],
             event->disc.addr.val[2], event->disc.addr.val[3], event->disc.addr.val[4],
             event->disc.addr.val[5]);

    wardriving_data.ble_data.ble_rssi = event->disc.rssi;

    // Parse BLE name if available
    if (event->disc.length_data > 0) {
        ble_hs_adv_parse(event->disc.data, event->disc.length_data, ble_hs_adv_parse_fields_cb,
                         &wardriving_data);
    }

    // Get GPS data from the global handle
    gps_t *gps = &((esp_gps_t *)nmea_hdl)->parent;
    if (gps != NULL && gps->valid) {
        wardriving_data.gps_quality.satellites_used = gps->sats_in_use;
        wardriving_data.gps_quality.hdop = gps->dop_h;
        wardriving_data.gps_quality.speed = gps->speed;
        wardriving_data.gps_quality.course = gps->cog;
        wardriving_data.gps_quality.fix_quality = gps->fix;
        wardriving_data.gps_quality.has_valid_fix = (gps->fix >= GPS_FIX_GPS);
    }

    // Use GPS manager to log data
    esp_err_t err = gps_manager_log_wardriving_data(&wardriving_data);
    if (err != ESP_OK) {
        ESP_LOGD("BLE_WD", "Skipped logging entry\nGPS data not ready");
    }
}

// Move the callback implementation inside the ESP32S2 guard
static int ble_hs_adv_parse_fields_cb(const struct ble_hs_adv_field *field, void *arg) {
    wardriving_data_t *data = (wardriving_data_t *)arg;

    if (field->type == BLE_HS_ADV_TYPE_COMP_NAME) {
        size_t name_len = MIN(field->length, sizeof(data->ble_data.ble_name) - 1);
        memcpy(data->ble_data.ble_name, field->value, name_len);
        data->ble_data.ble_name[name_len] = '\0';
    }

    return 0;
}
#endif

// wrap for esp32s2
#ifndef CONFIG_IDF_TARGET_ESP32S2

static const int suspicious_names_count = sizeof(suspicious_names) / sizeof(suspicious_names[0]);
void ble_skimmer_scan_callback(struct ble_gap_event *event, void *arg) {
    if (!event || event->type != BLE_GAP_EVENT_DISC) {
        return;
    }

    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

    if (rc != 0) {
        ESP_LOGD(SKIMMER_TAG, "Failed to parse advertisement data");
        return;
    }

    // Check device name
    if (fields.name != NULL && fields.name_len > 0) {
        char device_name[32] = {0};
        size_t name_len = MIN(fields.name_len, sizeof(device_name) - 1);
        memcpy(device_name, fields.name, name_len);

        // Check against suspicious names
        for (int i = 0; i < suspicious_names_count; i++) {
            if (strcasecmp(device_name, suspicious_names[i]) == 0) {
                char mac_addr[18];
                snprintf(mac_addr, sizeof(mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
                         event->disc.addr.val[0], event->disc.addr.val[1], event->disc.addr.val[2],
                         event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);

                IRAM_PRINTF("\nPOTENTIAL SKIMMER DETECTED!\n");
                TERMINAL_VIEW_ADD_TEXT("\nPOTENTIAL SKIMMER DETECTED!\n");

                IRAM_PRINTF("Device Name: %s\n", device_name);
                TERMINAL_VIEW_ADD_TEXT("Device Name: ");
                TERMINAL_VIEW_ADD_TEXT(device_name);
                TERMINAL_VIEW_ADD_TEXT("\n");

                IRAM_PRINTF("MAC Address: %s\n", mac_addr);
                TERMINAL_VIEW_ADD_TEXT("MAC Address: ");
                TERMINAL_VIEW_ADD_TEXT(mac_addr);
                TERMINAL_VIEW_ADD_TEXT("\n");

                IRAM_PRINTF("RSSI: %d dBm\n", event->disc.rssi);
                TERMINAL_VIEW_ADD_TEXT("RSSI: ");
                char rssi_str[12];
                snprintf(rssi_str, sizeof(rssi_str), "%d", event->disc.rssi);
                TERMINAL_VIEW_ADD_TEXT(rssi_str);
                TERMINAL_VIEW_ADD_TEXT(" dBm\n");

                IRAM_PRINTF("Reason:\nMatched known skimmer pattern: %s\n", suspicious_names[i]);
                TERMINAL_VIEW_ADD_TEXT("Reason:\nMatched known skimmer pattern: ");
                TERMINAL_VIEW_ADD_TEXT(suspicious_names[i]);
                TERMINAL_VIEW_ADD_TEXT("\n");

                IRAM_PRINTF("Please verify before taking action.\n\n");
                TERMINAL_VIEW_ADD_TEXT("Please verify before taking action.\n\n");

                // pulse rgb red once when skimmer is detected
                pulse_once(&rgb_manager, 255, 0, 0);

                // Create enhanced PCAP packet with metadata
                if (pcap_file != NULL) {
                    // Format: [Timestamp][MAC][RSSI][Name][Raw Data]
                    uint8_t enhanced_packet[256] = {0};
                    size_t packet_len = 0;

                    // Add MAC address
                    memcpy(enhanced_packet + packet_len, event->disc.addr.val, 6);
                    packet_len += 6;

                    // Add RSSI
                    enhanced_packet[packet_len++] = (uint8_t)event->disc.rssi;

                    // Add device name length and name
                    enhanced_packet[packet_len++] = (uint8_t)name_len;
                    memcpy(enhanced_packet + packet_len, device_name, name_len);
                    packet_len += name_len;

                    // Add reason for flagging
                    const char *reason = suspicious_names[i];
                    uint8_t reason_len = strlen(reason);
                    enhanced_packet[packet_len++] = reason_len;
                    memcpy(enhanced_packet + packet_len, reason, reason_len);
                    packet_len += reason_len;

                    // Add raw advertisement data
                    memcpy(enhanced_packet + packet_len, event->disc.data, event->disc.length_data);
                    packet_len += event->disc.length_data;

                    // Write to PCAP with proper BLE packet format
                    pcap_write_packet_to_buffer(enhanced_packet, packet_len,
                                                PCAP_CAPTURE_BLUETOOTH);

                    // Force flush to ensure suspicious device is captured
                    pcap_flush_buffer_to_file();
                }
                break;
            }
        }
    }
}
#endif
