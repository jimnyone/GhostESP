/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <sys/param.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "core/dns_server.h"
<<<<<<< HEAD
=======
#include "managers/ghostscript_runtime.h"
#include "managers/sd_card_manager.h"
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#define DNS_PORT (53)
#define DNS_MAX_LEN (256)

#define OPCODE_MASK (0x7800)
#define QR_FLAG (1 << 7)
#define QD_TYPE_A (0x0001)
#define ANS_TTL_SEC (300)

static const char *TAG = "dns_redirect_server";

// DNS Header Packet
typedef struct __attribute__((__packed__)) {
  uint16_t id;
  uint16_t flags;
  uint16_t qd_count;
  uint16_t an_count;
  uint16_t ns_count;
  uint16_t ar_count;
} dns_header_t;

// DNS Question Packet
typedef struct {
  uint16_t type;
  uint16_t class;
} dns_question_t;

// DNS Answer Packet
typedef struct __attribute__((__packed__)) {
  uint16_t ptr_offset;
  uint16_t type;
  uint16_t class;
  uint32_t ttl;
  uint16_t addr_len;
  uint32_t ip_addr;
} dns_answer_t;

// DNS server handle
struct dns_server_handle {
  bool started;
  TaskHandle_t task;
  int num_of_entries;
  dns_entry_pair_t entry[];
};

/*
    Parse the name from the packet from the DNS name format to a regular
   .-seperated name returns the pointer to the next part of the packet
*/
static char *parse_dns_name(char *raw_name, char *parsed_name,
                            size_t parsed_name_max_len) {

  char *label = raw_name;
  char *name_itr = parsed_name;
  int name_len = 0;

  do {
    int sub_name_len = *label;
    // (len + 1) since we are adding  a '.'
    name_len += (sub_name_len + 1);
    if (name_len > parsed_name_max_len) {
      return NULL;
    }

    // Copy the sub name that follows the the label
    memcpy(name_itr, label + 1, sub_name_len);
    name_itr[sub_name_len] = '.';
    name_itr += (sub_name_len + 1);
    label += sub_name_len + 1;
  } while (*label != 0);

  // Terminate the final string, replacing the last '.'
  parsed_name[name_len - 1] = '\0';
  // Return pointer to first char after the name
  return label + 1;
}

// Parses the DNS request and prepares a DNS response with the IP of the softAP
static int parse_dns_request(char *req, size_t req_len, char *dns_reply,
                             size_t dns_reply_max_len, dns_server_handle_t h) {
  if (req_len > dns_reply_max_len) {
    return -1;
  }

  // Prepare the reply
  memset(dns_reply, 0, dns_reply_max_len);
  memcpy(dns_reply, req, req_len);

  // Endianess of NW packet different from chip
  dns_header_t *header = (dns_header_t *)dns_reply;
  ESP_LOGD(TAG, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d",
           ntohs(header->id), ntohs(header->flags), ntohs(header->qd_count));

  // Not a standard query
  if ((header->flags & OPCODE_MASK) != 0) {
    return 0;
  }

  // Set question response flag
  header->flags |= QR_FLAG;

  uint16_t qd_count = ntohs(header->qd_count);
  header->an_count = htons(qd_count);

  int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
  if (reply_len > dns_reply_max_len) {
    return -1;
  }

  // Pointer to current answer and question
  char *cur_ans_ptr = dns_reply + req_len;
  char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
  char name[128];

  // Respond to all questions based on configured rules
  for (int qd_i = 0; qd_i < qd_count; qd_i++) {
    char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
    if (name_end_ptr == NULL) {
      ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
      return -1;
    }

    dns_question_t *question = (dns_question_t *)(name_end_ptr);
    uint16_t qd_type = ntohs(question->type);
    uint16_t qd_class = ntohs(question->class);

    ESP_LOGD(TAG, "Received type: %d | Class: %d | Question for: %s", qd_type,
             qd_class, name);

    if (qd_type == QD_TYPE_A) {
      esp_ip4_addr_t ip = {.addr = IPADDR_ANY};
      // Check the configured rules to decide whether to answer this question or
      // not
      for (int i = 0; i < h->num_of_entries; ++i) {
        // check if the name either corresponds to the entry, or if we should
        // answer to all queries ("*")
        if (strcmp(h->entry[i].name, "*") == 0 ||
            strcmp(h->entry[i].name, name) == 0) {
          if (h->entry[i].if_key) {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(
                esp_netif_get_handle_from_ifkey(h->entry[i].if_key), &ip_info);
            ip.addr = ip_info.ip.addr;
            break;
          } else if (h->entry->ip.addr != IPADDR_ANY) {
            ip.addr = h->entry[i].ip.addr;
            break;
          }
        }
      }
      if (ip.addr ==
          IPADDR_ANY) { // no rule applies, continue with another question
        continue;
      }
      dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;

      answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
      answer->type = htons(qd_type);
      answer->class = htons(qd_class);
      answer->ttl = htonl(ANS_TTL_SEC);

      ESP_LOGD(TAG, "Answer with PTR offset: 0x%" PRIX16 " and IP 0x%" PRIX32,
               ntohs(answer->ptr_offset), ip.addr);

      answer->addr_len = htons(sizeof(ip.addr));
      answer->ip_addr = ip.addr;
    }
  }
  return reply_len;
}

/*
    Sets up a socket and listen for DNS queries,
    replies to all type A queries with the IP of the softAP
*/
void dns_server_task(void *pvParameters) {
  char rx_buffer[128];
  char addr_str[128];
  int addr_family;
  int ip_protocol;
  dns_server_handle_t handle = pvParameters;

  while (handle->started) {

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DNS_PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket bound, port %d", DNS_PORT);

    while (handle->started) {
      ESP_LOGI(TAG, "Waiting for data");
      struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
      socklen_t socklen = sizeof(source_addr);
      int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                         (struct sockaddr *)&source_addr, &socklen);

      // Error occurred during receiving
      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        close(sock);
        break;
      }
      // Data received
      else {
        // Get the sender's ip address as string
        if (source_addr.sin6_family == PF_INET) {
          inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr,
                      addr_str, sizeof(addr_str) - 1);
        } else if (source_addr.sin6_family == PF_INET6) {
          inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }

        // Null-terminate whatever we received and treat like a string...
        rx_buffer[len] = 0;

        char reply[DNS_MAX_LEN];
        int reply_len =
            parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN, handle);

        ESP_LOGI(TAG, "Received %d bytes from %s | DNS reply with len: %d", len,
                 addr_str, reply_len);
        if (reply_len <= 0) {
          ESP_LOGE(TAG, "Failed to prepare a DNS reply");
        } else {
<<<<<<< HEAD
          int err =
              sendto(sock, reply, reply_len, 0, (struct sockaddr *)&source_addr,
                     sizeof(source_addr));
          if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
          }
=======
            const char *lookup_mode = handle->use_psram_path ? "PSRAM bloom+ht" :
                                      handle->bloom ? "SD bloom+binary verify" :
                                      "raw SD binary search";
            glog("DNS sinkhole: ready (%s, upstream " IPSTR ")\n",
                 lookup_mode,
                 IP2STR(&(esp_ip4_addr_t){.addr = handle->upstream_dns}));
        }

        while (handle->started) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(listen_sock, &readfds);
            FD_SET(fwd_sock, &readfds);
            int maxfd = listen_sock > fwd_sock ? listen_sock : fwd_sock;

            struct timeval sel_tv = {1, 0};
            int sel = select(maxfd + 1, &readfds, NULL, NULL, &sel_tv);
            if (sel <= 0) continue;

            // Handle upstream responses first (non-blocking)
            if (FD_ISSET(fwd_sock, &readfds)) {
                char resp[DNS_MAX_LEN];
                struct sockaddr_in resp_src;
                socklen_t resp_len = sizeof(resp_src);
                int rlen = recvfrom(fwd_sock, resp, sizeof(resp), 0,
                                    (struct sockaddr *)&resp_src, &resp_len);
                if (rlen >= (int)sizeof(dns_header_t)) {
                    if (resp_src.sin_addr.s_addr != upstream_addr.sin_addr.s_addr ||
                        resp_src.sin_port != upstream_addr.sin_port) {
                        continue;
                    }
                    uint16_t resp_id = ntohs(((dns_header_t *)resp)->id);
                    int ri = fwd_ring_find(fwd_ring, SINKHOLE_FWD_RING, resp_id);
                    if (ri >= 0) {
                        ((dns_header_t *)resp)->id = htons(fwd_ring[ri].client_id);
                        char matched_domain[SINKHOLE_MAX_DOMAIN] = {0};
                        if (response_has_blocked_cname(handle, resp, rlen,
                                                       matched_domain,
                                                       sizeof(matched_domain))) {
                            send_nxdomain(listen_sock, resp, rlen,
                                          &fwd_ring[ri].client_addr);
                            handle->stat_blocked++;
                            char cip[INET_ADDRSTRLEN];
                            inet_ntoa_r(fwd_ring[ri].client_addr.sin_addr, cip,
                                        sizeof(cip) - 1);
                            log_query(handle, cip, fwd_ring[ri].domain,
                                      fwd_ring[ri].qtype, "BLOCKED_CNAME",
                                      matched_domain);
                        } else {
                            sendto(listen_sock, resp, rlen, 0,
                                   (struct sockaddr *)&fwd_ring[ri].client_addr,
                                   sizeof(fwd_ring[ri].client_addr));
                        }
                        fwd_ring[ri].used = false;
                    }
                }
            }

            // Handle incoming client queries
            if (!FD_ISSET(listen_sock, &readfds)) continue;

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int len = recvfrom(listen_sock, rx_buffer, sizeof(rx_buffer), 0,
                               (struct sockaddr *)&client_addr, &client_len);
            if (len < 0 || len < (int)sizeof(dns_header_t)) continue;

            dns_header_t *hdr = (dns_header_t *)rx_buffer;
            uint16_t flags = ntohs(hdr->flags);
            if ((flags & OPCODE_MASK) != 0) continue;
            if (flags & 0x8000) continue;
            if (ntohs(hdr->qd_count) == 0) continue;

            char name[SINKHOLE_MAX_DOMAIN];
            char *name_end = parse_dns_name(
                rx_buffer + sizeof(dns_header_t), rx_buffer + len,
                name, sizeof(name));
            if (!name_end) continue;
            if (name_end + sizeof(dns_question_t) > rx_buffer + len) continue;
            dns_question_t *question = (dns_question_t *)name_end;
            uint16_t qtype = ntohs(question->type);

            for (char *p = name; *p; p++) *p = (char)tolower((uint8_t)*p);

            handle->stat_total++;
            if (handle->stat_total == 1) {
                glog("DNS sinkhole: first query received (%s)\n", name);
            } else if (handle->stat_total == 2) {
                glog("DNS sinkhole: forwarding to upstream " IPSTR "\n",
                     IP2STR(&(esp_ip4_addr_t){.addr = handle->upstream_dns}));
            } else if (handle->stat_total % 50 == 0) {
                glog("Sinkhole: %lu queries, %lu blocked, %lu dropped\n",
                     (unsigned long)handle->stat_total,
                     (unsigned long)handle->stat_blocked,
                     (unsigned long)handle->stat_dropped);
                if (handle->auto_stats) {
                    char sbuf[256];
                    int sn = snprintf(sbuf, sizeof(sbuf),
                        "total=%lu\nblocked=%lu\ndropped=%lu\nupstream=" IPSTR "\nlog=%s\n",
                        (unsigned long)handle->stat_total,
                        (unsigned long)handle->stat_blocked,
                        (unsigned long)handle->stat_dropped,
                        IP2STR(&(esp_ip4_addr_t){.addr = handle->upstream_dns}),
                        handle->log_enabled ? "on" : "off");
                    if (sn > 0) sd_card_write_file(SINKHOLE_STATS_PATH, sbuf, (size_t)sn);
                }
            }
            uint32_t hash = fnv1a_hash(name);

            bool blocked = false;
            char matched_domain[SINKHOLE_MAX_DOMAIN] = {0};
            bool cache_hit = cache_check(handle, name, hash, &blocked,
                                         matched_domain,
                                         sizeof(matched_domain));

            if (!cache_hit) {
                blocked = blocklist_check(handle, name, matched_domain,
                                          sizeof(matched_domain));
                cache_insert(handle, name, hash, blocked, matched_domain);
            }

            if (blocked) {
                send_blocked_response(listen_sock, rx_buffer, len,
                                      &client_addr, handle);
                handle->stat_blocked++;

                char cip[INET_ADDRSTRLEN];
                inet_ntoa_r(client_addr.sin_addr, cip, sizeof(cip) - 1);
                log_query(handle, cip, name, qtype, "BLOCKED", matched_domain);
            } else {
                uint16_t client_id = ntohs(hdr->id);
                int slot = -1;
                for (int i = 0; i < SINKHOLE_FWD_RING; i++) {
                    if (!fwd_ring[i].used) { slot = i; break; }
                }
                if (slot < 0) {
                    handle->stat_dropped++;
                    if ((handle->stat_dropped & 0xFF) == 1) {
                        glog("Sinkhole: forward ring full, dropping query for %s\n", name);
                    }
                    continue;
                }

                uint16_t upstream_id = ++next_fwd_id;
                if (upstream_id == 0) upstream_id = ++next_fwd_id;
                fwd_ring[slot].upstream_id = upstream_id;
                fwd_ring[slot].client_id = client_id;
                fwd_ring[slot].qtype = qtype;
                fwd_ring[slot].client_addr = client_addr;
                snprintf(fwd_ring[slot].domain, sizeof(fwd_ring[slot].domain),
                         "%s", name);
                fwd_ring[slot].used = true;

                hdr->id = htons(upstream_id);
                sendto(fwd_sock, rx_buffer, len, 0,
                       (struct sockaddr *)&upstream_addr,
                       sizeof(upstream_addr));

                char cip[INET_ADDRSTRLEN];
                inet_ntoa_r(client_addr.sin_addr, cip, sizeof(cip) - 1);
                log_query(handle, cip, name, qtype, "FORWARDED", NULL);
            }
        }

        close(fwd_sock);
        close(listen_sock);

        if (handle->use_psram_path) {
            free_sinkhole_psram(handle);
        } else if (handle->bloom) {
            heap_caps_free(handle->bloom);
            handle->bloom = NULL;
            handle->bloom_size_bytes = 0;
        }
        if (handle->blocklist_fp) {
            fclose(handle->blocklist_fp);
            handle->blocklist_fp = NULL;
        }

        if (handle->sinkhole_jit_mounted) {
            sd_card_unmount_after_flush(handle->sinkhole_display_suspended);
            handle->sinkhole_jit_mounted = false;
        }
    } else {
        // --- Legacy evil portal DNS mode ---
        char addr_str[128];

        while (handle->started) {
            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(DNS_PORT);
            inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

            int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
                break;
            }

            int err = bind(sock, (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
            if (err < 0)
                ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);

            while (handle->started) {
                struct sockaddr_in6 source_addr;
                socklen_t socklen = sizeof(source_addr);
                int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                                   (struct sockaddr *)&source_addr, &socklen);

                if (len < 0) {
                    close(sock);
                    break;
                } else {
                    if (source_addr.sin6_family == PF_INET) {
                        inet_ntoa_r(
                            ((struct sockaddr_in *)&source_addr)->sin_addr.s_addr,
                            addr_str, sizeof(addr_str) - 1);
                    } else if (source_addr.sin6_family == PF_INET6) {
                        inet6_ntoa_r(source_addr.sin6_addr, addr_str,
                                     sizeof(addr_str) - 1);
                    }

                    char reply[DNS_MAX_LEN];
                    int reply_len = parse_dns_request(rx_buffer, len, reply,
                                                      DNS_MAX_LEN, handle);
                    if (reply_len > 0) {
                        sendto(sock, reply, reply_len, 0,
                               (struct sockaddr *)&source_addr,
                               sizeof(source_addr));
                    }
                    /* Emit a structured event for the qname if we can extract it. */
                    if (len >= 17) {
                        char qname[128];
                        int qpos = 0;
                        int i = 12;
                        while (i < len && rx_buffer[i] != 0 && qpos < (int)sizeof(qname) - 1) {
                            uint8_t lbl = rx_buffer[i++];
                            if (lbl > 63) break;
                            for (int k = 0; k < lbl && i < len && qpos < (int)sizeof(qname) - 1; ++k)
                                qname[qpos++] = rx_buffer[i++];
                            if (qpos < (int)sizeof(qname) - 1) qname[qpos++] = '.';
                        }
                        if (qpos > 0) qname[qpos - 1] = '\0'; else qname[0] = '\0';
                        if (qname[0]) {
                            char dns_payload[300];
                            snprintf(dns_payload, sizeof(dns_payload), "%s|%s",
                                addr_str, qname);
                            ghostscript_emit_event_escaped("dns_request", dns_payload);
                        }
                    }
                }
            }

            if (sock != -1) {
                shutdown(sock, 0);
                close(sock);
            }
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
        }
      }
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket");
      shutdown(sock, 0);
      close(sock);
    }
  }
  vTaskDelete(NULL);
}

dns_server_handle_t start_dns_server(dns_server_config_t *config) {
  dns_server_handle_t handle =
      calloc(1, sizeof(struct dns_server_handle) +
                    config->num_of_entries * sizeof(dns_entry_pair_t));
  ESP_RETURN_ON_FALSE(handle, NULL, TAG,
                      "Failed to allocate dns server handle");

  handle->started = true;
  handle->num_of_entries = config->num_of_entries;
  memcpy(handle->entry, config->item,
         config->num_of_entries * sizeof(dns_entry_pair_t));

  xTaskCreate(dns_server_task, "dns_server", 4096, handle, 5, &handle->task);
  return handle;
}

void stop_dns_server(dns_server_handle_t handle) {
  if (handle) {
    handle->started = false;
    vTaskDelete(handle->task);
    free(handle);
  }
}