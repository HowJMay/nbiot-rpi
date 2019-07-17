#include "pub_utils.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "config.h"

bool first_publish = true;
int last_mid_sent = -1;
char *line_buf = NULL;
int line_buf_len = 1024;
bool connected = true;
bool disconnect_sent = false;
int publish_count = 0;
bool ready_for_repeat = false;
struct timeval next_publish_tv;

static void set_repeat_time(mosq_config_t *cfg) {
  gettimeofday(&next_publish_tv, NULL);
  next_publish_tv.tv_sec += cfg->pub_config->repeat_delay.tv_sec;
  next_publish_tv.tv_usec += cfg->pub_config->repeat_delay.tv_usec;

  next_publish_tv.tv_sec += next_publish_tv.tv_usec / 1e6;
  next_publish_tv.tv_usec = next_publish_tv.tv_usec % 1000000;
}

static int check_repeat_time(void) {
  struct timeval tv;

  gettimeofday(&tv, NULL);

  if (tv.tv_sec > next_publish_tv.tv_sec) {
    return EXIT_FAILURE;
  } else if (tv.tv_sec == next_publish_tv.tv_sec && tv.tv_usec > next_publish_tv.tv_usec) {
    return EXIT_FAILURE;
  }
  return 0;
}

int publish_message(struct mosquitto *mosq, mosq_config_t *cfg, int *mid, const char *topic, int payloadlen, void *payload, int qos,
                    bool retain) {
  ready_for_repeat = false;
  if (cfg->general_config->protocol_version == MQTT_PROTOCOL_V5 && cfg->pub_config->have_topic_alias &&
      first_publish == false) {
    return mosquitto_publish_v5(mosq, mid, NULL, payloadlen, payload, qos, retain, cfg->property_config->publish_props);
  } else {
    first_publish = false;
    return mosquitto_publish_v5(mosq, mid, topic, payloadlen, payload, qos, retain, cfg->property_config->publish_props);
  }
}

void log_callback_pub_func(struct mosquitto *mosq, void *obj, int level, const char *str) {
  
  UNUSED(level);

  printf("log: [%s]\n", str);
}

void disconnect_callback_pub_func(struct mosquitto *mosq, void *obj, mosq_retcode_t ret,
                                  const mosquitto_property *properties) {
  
  
  UNUSED(ret);
  UNUSED(properties);

  connected = false;
  fprintf(stdout, "Publisher disconnect pub callback.\n");
}

void connect_callback_pub_func(struct mosquitto *mosq, void *obj, int result, int flags,
                               const mosquitto_property *properties) {
  mosq_retcode_t ret = MOSQ_ERR_SUCCESS;
  mosq_config_t *cfg = (mosq_config_t *)obj;
  UNUSED(flags);
  UNUSED(properties);

  if (!result) {
    switch (cfg->pub_config->pub_mode) {
      case MSGMODE_CMD:
      case MSGMODE_FILE:
      case MSGMODE_STDIN_FILE:
        ret = publish_message(mosq, cfg, &mid_sent, cfg->pub_config->topic, cfg->pub_config->msglen, cfg->pub_config->message,
                              cfg->general_config->qos, cfg->general_config->retain);
        break;
      case MSGMODE_NULL:
        ret = publish_message(mosq, cfg, &mid_sent, cfg->pub_config->topic, 0, NULL, cfg->general_config->qos,
                              cfg->general_config->retain);
        break;
      case MSGMODE_STDIN_LINE:
        status = STATUS_CONNACK_RECVD;
        break;
    }
    if (ret) {
      if (!cfg->general_config->quiet) {
        switch (ret) {
          case MOSQ_ERR_INVAL:
            fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
            break;
          case MOSQ_ERR_NOMEM:
            fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
            break;
          case MOSQ_ERR_NO_CONN:
            fprintf(stderr, "Error: Client not connected when trying to publish.\n");
            break;
          case MOSQ_ERR_PROTOCOL:
            fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
            break;
          case MOSQ_ERR_PAYLOAD_SIZE:
            fprintf(stderr, "Error: Message payload is too large.\n");
            break;
          case MOSQ_ERR_QOS_NOT_SUPPORTED:
            fprintf(stderr, "Error: Message QoS not supported on broker, try a lower QoS.\n");
            break;
        }
      }
      mosquitto_disconnect_v5(mosq, 0, cfg->property_config->disconnect_props);
    }
  } else {
    if (result && !cfg->general_config->quiet) {
      if (cfg->general_config->protocol_version == MQTT_PROTOCOL_V5) {
        fprintf(stderr, "%s\n", mosquitto_reason_string(result));
      } else {
        fprintf(stderr, "%s\n", mosquitto_connack_string(result));
      }
    }
  }
  fprintf(stdout, "Publisher connect pub callback.\n");
}

void publish_callback_pub_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                               const mosquitto_property *properties) {
  mosq_config_t *cfg = (mosq_config_t *)obj;
  UNUSED(properties);

  last_mid_sent = mid;
  if (reason_code > 127) {
    if (!cfg->general_config->quiet) {
      fprintf(stderr, "Warning: Publish %d failed: %s.\n", mid, mosquitto_reason_string(reason_code));
    }
  }
  publish_count++;

  if (cfg->pub_config->pub_mode == MSGMODE_STDIN_LINE) {
    if (mid == last_mid) {
      mosquitto_disconnect_v5(mosq, 0, cfg->property_config->disconnect_props);
      disconnect_sent = true;
    }
  } else if (publish_count < cfg->pub_config->repeat_count) {
    ready_for_repeat = true;
    set_repeat_time(cfg);
  } else if (disconnect_sent == false) {
    mosquitto_disconnect_v5(mosq, 0, cfg->property_config->disconnect_props);
    disconnect_sent = true;
  }

  fprintf(stdout, "Publisher publish pub callback.\n");
}

int pub_shared_init(void) {
  line_buf = malloc(line_buf_len);
  if (!line_buf) {
    fprintf(stderr, "Error: Out of memory.\n");
    return EXIT_FAILURE;
  }
  return 0;
}

int publish_loop(struct mosquitto *mosq, mosq_config_t *cfg) {
  int read_len;
  int pos;
  mosq_retcode_t ret, ret2;
  char *buf2;
  int buf_len_actual;
  int mode;
  int loop_delay = 1000;

  if (cfg->pub_config->repeat_count > 1 &&
      (cfg->pub_config->repeat_delay.tv_sec == 0 || cfg->pub_config->repeat_delay.tv_usec != 0)) {
    loop_delay = cfg->pub_config->repeat_delay.tv_usec / 2000;
  }

  mode = cfg->pub_config->pub_mode;

  if (mode == MSGMODE_STDIN_LINE) {
    mosquitto_loop_start(mosq);
  }

  do {
    if (mode == MSGMODE_STDIN_LINE) {
      if (status == STATUS_CONNACK_RECVD) {
        pos = 0;
        read_len = line_buf_len;
        while (connected && fgets(&line_buf[pos], read_len, stdin)) {
          buf_len_actual = strlen(line_buf);
          if (line_buf[buf_len_actual - 1] == '\n') {
            line_buf[buf_len_actual - 1] = '\0';
            ret2 = publish_message(mosq, cfg, &mid_sent, cfg->pub_config->topic, buf_len_actual - 1, line_buf,
                                   cfg->general_config->qos, cfg->general_config->retain);
            if (ret2) {
              if (!cfg->general_config->quiet) fprintf(stderr, "Error: Publish returned %d, disconnecting.\n", ret2);
              mosquitto_disconnect_v5(mosq, MQTT_RC_DISCONNECT_WITH_WILL_MSG, cfg->property_config->disconnect_props);
            }
            break;
          } else {
            line_buf_len += 1024;
            pos += 1023;
            read_len = 1024;
            buf2 = realloc(line_buf, line_buf_len);
            if (!buf2) {
              fprintf(stderr, "Error: Out of memory.\n");
              return MOSQ_ERR_NOMEM;
            }
            line_buf = buf2;
          }
        }
        if (feof(stdin)) {
          if (mid_sent == -1) {
            /* Empty file */
            mosquitto_disconnect_v5(mosq, 0, cfg->property_config->disconnect_props);
            disconnect_sent = true;
            status = STATUS_DISCONNECTING;
          } else {
            last_mid = mid_sent;
            status = STATUS_WAITING;
          }
        }
      } else if (status == STATUS_WAITING) {
        if (last_mid_sent == last_mid && disconnect_sent == false) {
          mosquitto_disconnect_v5(mosq, 0, cfg->property_config->disconnect_props);
          disconnect_sent = true;
        }
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000;
        nanosleep(&ts, NULL);
      }
      ret = MOSQ_ERR_SUCCESS;
    } else {
      ret = mosquitto_loop(mosq, loop_delay, 1);
      if (ready_for_repeat && check_repeat_time()) {
        ret = 0;
        switch (cfg->pub_config->pub_mode) {
          case MSGMODE_CMD:
          case MSGMODE_FILE:
          case MSGMODE_STDIN_FILE:
            ret = publish_message(mosq, cfg, &mid_sent, cfg->pub_config->topic, cfg->pub_config->msglen,
                                  cfg->pub_config->message, cfg->general_config->qos, cfg->general_config->retain);
            break;
          case MSGMODE_NULL:
            ret = publish_message(mosq, cfg, &mid_sent, cfg->pub_config->topic, 0, NULL, cfg->general_config->qos,
                                  cfg->general_config->retain);
            break;
          case MSGMODE_STDIN_LINE:
            break;
        }
        if (ret) {
          fprintf(stderr, "Error sending repeat publish: %s", mosquitto_strerror(ret));
        }
      }
    }
  } while (ret == MOSQ_ERR_SUCCESS && connected);

  if (mode == MSGMODE_STDIN_LINE) {
    mosquitto_loop_stop(mosq, false);
  }
  return 0;
}

mosq_retcode_t init_check_error(mosq_config_t *cfg, client_type_t client_type) {
  rc_mosq_retcode_t ret = RC_MOS_OK;

  if (cfg->general_config->will_payload && !cfg->general_config->will_topic) {
    fprintf(stderr, "Error: Will payload given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->general_config->will_retain && !cfg->general_config->will_topic) {
    fprintf(stderr, "Error: Will retain given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->general_config->password && !cfg->general_config->username) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Warning: Not using password since username not set.\n");
  }

#ifndef WITH_THREADING
  if (cfg->general_config->clean_session == MSGMODE_STDIN_LINE) {
    fprintf(stderr, "Error: '-l' mode not available, threading support has not been compiled in.\n");
    return RC_MOS_INIT_ERROR;
  }
#endif

  if (!cfg->pub_config->topic || cfg->general_config->clean_session == MSGMODE_NONE) {
    fprintf(stderr, "Error: Both topic and message must be supplied.\n");
    return RC_MOS_INIT_ERROR;
  }

  if (cfg->general_config->will_payload && !cfg->general_config->will_topic) {
    fprintf(stderr, "Error: Will payload given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->general_config->will_retain && !cfg->general_config->will_topic) {
    fprintf(stderr, "Error: Will retain given, but no will topic given.\n");
    return EXIT_FAILURE;
  }
  if (cfg->general_config->password && !cfg->general_config->username) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Warning: Not using password since username not set.\n");
  }
#ifdef WITH_TLS
  if ((cfg->tls_config->certfile && !cfg->tls_config->keyfile) ||
      (cfg->tls_config->keyfile && !cfg->tls_config->certfile)) {
    fprintf(stderr, "Error: Both certfile and keyfile must be provided if one of them is set.\n");
    return EXIT_FAILURE;
  }
  if ((cfg->tls_config->keyform && !cfg->tls_config->keyfile)) {
    fprintf(stderr, "Error: If keyform is set, keyfile must be also specified.\n");
    return EXIT_FAILURE;
  }
  if ((cfg->tls_config->tls_engine_kpass_sha1 && (!cfg->tls_config->keyform || !cfg->tls_config->tls_engine))) {
    fprintf(stderr, "Error: when using tls-engine-kpass-sha1, both tls-engine and keyform must also be provided.\n");
    return EXIT_FAILURE;
  }
#endif
#ifdef FINAL_WITH_TLS_PSK
  if ((cfg->tls_config->cafile || cfg->tls_config->capath) && cfg->tls_config->psk) {
    if (!cfg->general_config->quiet)
      fprintf(stderr, "Error: Only one of --psk or --cafile/--capath may be used at once.\n");
    return EXIT_FAILURE;
  }
  if (cfg->tls_config->psk && !cfg->tls_config->psk_identity) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error: --psk-identity required if --psk used.\n");
    return EXIT_FAILURE;
  }
#endif

  if (cfg->general_config->clean_session == false && (cfg->general_config->id_prefix || !cfg->general_config->id)) {
    if (!cfg->general_config->quiet)
      fprintf(stderr, "Error: You must provide a client id if you are using the -c option.\n");
    return EXIT_FAILURE;
  }

  if (client_type == client_sub) {
    if (cfg->sub_config->topic_count == 0) {
      if (!cfg->general_config->quiet) fprintf(stderr, "Error: You must specify a topic to subscribe to.\n");
      return EXIT_FAILURE;
    }
  }

  if (!cfg->general_config->host) {
    cfg->general_config->host = strdup("localhost");
    if (!cfg->general_config->host) {
      if (!cfg->general_config->quiet) fprintf(stderr, "Error: Out of memory.\n");
      return EXIT_FAILURE;
    }
  }

  ret = mosquitto_property_check_all(CMD_CONNECT, cfg->property_config->connect_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in CONNECT properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_PUBLISH, cfg->property_config->publish_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in PUBLISH properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_SUBSCRIBE, cfg->property_config->subscribe_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in SUBSCRIBE properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_UNSUBSCRIBE, cfg->property_config->unsubscribe_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in UNSUBSCRIBE properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_DISCONNECT, cfg->property_config->disconnect_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in DISCONNECT properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }
  ret = mosquitto_property_check_all(CMD_WILL, cfg->property_config->will_props);
  if (ret) {
    if (!cfg->general_config->quiet) fprintf(stderr, "Error in Will properties: %s\n", mosquitto_strerror(ret));
    return EXIT_FAILURE;
  }

  return MOSQ_ERR_SUCCESS;
}

void pub_shared_cleanup(void) { free(line_buf); }