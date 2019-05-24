#include "sub_utils.h"
//#include "sub_client_output.h"

struct mosq_config cfg;
bool process_messages = true;
int msg_count = 0;
struct mosquitto *mosq = NULL;
int last_mid = 0;

static int get_time(struct tm **ti, long *ns) {
  struct timespec ts;
  time_t s;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    fprintf(stderr, "Error obtaining system time.\n");
    return 1;
  }
  s = ts.tv_sec;
  *ns = ts.tv_nsec;

  *ti = localtime(&s);
  if (!(*ti)) {
    fprintf(stderr, "Error obtaining system time.\n");
    return 1;
  }

  return 0;
}

static void write_payload(const unsigned char *payload, int payloadlen, int hex) {
  int i;

  if (hex == 0) {
    (void)fwrite(payload, 1, payloadlen, stdout);
  } else if (hex == 1) {
    for (i = 0; i < payloadlen; i++) {
      fprintf(stdout, "%02x", payload[i]);
    }
  } else if (hex == 2) {
    for (i = 0; i < payloadlen; i++) {
      fprintf(stdout, "%02X", payload[i]);
    }
  }
}

static void write_json_payload(const char *payload, int payloadlen) {
  int i;

  for (i = 0; i < payloadlen; i++) {
    if (payload[i] == '"' || payload[i] == '\\' || (payload[i] >= 0 && payload[i] < 32)) {
      printf("\\u%04x", payload[i]);
    } else {
      fputc(payload[i], stdout);
    }
  }
}

static void json_print(const struct mosquitto_message *message, const struct tm *ti, bool escaped) {
  char buf[100];

  strftime(buf, 100, "%s", ti);
  printf("{\"tst\":%s,\"topic\":\"%s\",\"qos\":%d,\"retain\":%d,\"payloadlen\":%d,", buf, message->topic, message->qos,
         message->retain, message->payloadlen);
  if (message->qos > 0) {
    printf("\"mid\":%d,", message->mid);
  }
  if (escaped) {
    fputs("\"payload\":\"", stdout);
    write_json_payload(message->payload, message->payloadlen);
    fputs("\"}", stdout);
  } else {
    fputs("\"payload\":", stdout);
    write_payload(message->payload, message->payloadlen, 0);
    fputs("}", stdout);
  }
}

static void formatted_print(const struct mosq_config *cfg, const struct mosquitto_message *message) {
  int len;
  int i;
  struct tm *ti = NULL;
  long ns;
  char strf[3];
  char buf[100];

  len = strlen(cfg->format);

  for (i = 0; i < len; i++) {
    if (cfg->format[i] == '%') {
      if (i < len - 1) {
        i++;
        switch (cfg->format[i]) {
          case '%':
            fputc('%', stdout);
            break;

          case 'I':
            if (!ti) {
              if (get_time(&ti, &ns)) {
                fprintf(stderr, "Error obtaining system time.\n");
                return;
              }
            }
            if (strftime(buf, 100, "%FT%T%z", ti) != 0) {
              fputs(buf, stdout);
            }
            break;

          case 'j':
            if (!ti) {
              if (get_time(&ti, &ns)) {
                fprintf(stderr, "Error obtaining system time.\n");
                return;
              }
            }
            json_print(message, ti, true);
            break;

          case 'J':
            if (!ti) {
              if (get_time(&ti, &ns)) {
                fprintf(stderr, "Error obtaining system time.\n");
                return;
              }
            }
            json_print(message, ti, false);
            break;

          case 'l':
            printf("%d", message->payloadlen);
            break;

          case 'm':
            printf("%d", message->mid);
            break;

          case 'p':
            write_payload(message->payload, message->payloadlen, 0);
            break;

          case 'q':
            fputc(message->qos + 48, stdout);
            break;

          case 'r':
            if (message->retain) {
              fputc('1', stdout);
            } else {
              fputc('0', stdout);
            }
            break;

          case 't':
            fputs(message->topic, stdout);
            break;

          case 'U':
            if (!ti) {
              if (get_time(&ti, &ns)) {
                fprintf(stderr, "Error obtaining system time.\n");
                return;
              }
            }
            if (strftime(buf, 100, "%s", ti) != 0) {
              printf("%s.%09ld", buf, ns);
            }
            break;

          case 'x':
            write_payload(message->payload, message->payloadlen, 1);
            break;

          case 'X':
            write_payload(message->payload, message->payloadlen, 2);
            break;
        }
      }
    } else if (cfg->format[i] == '@') {
      if (i < len - 1) {
        i++;
        if (cfg->format[i] == '@') {
          fputc('@', stdout);
        } else {
          if (!ti) {
            if (get_time(&ti, &ns)) {
              fprintf(stderr, "Error obtaining system time.\n");
              return;
            }
          }

          strf[0] = '%';
          strf[1] = cfg->format[i];
          strf[2] = 0;

          if (cfg->format[i] == 'N') {
            printf("%09ld", ns);
          } else {
            if (strftime(buf, 100, strf, ti) != 0) {
              fputs(buf, stdout);
            }
          }
        }
      }
    } else if (cfg->format[i] == '\\') {
      if (i < len - 1) {
        i++;
        switch (cfg->format[i]) {
          case '\\':
            fputc('\\', stdout);
            break;

          case '0':
            fputc('\0', stdout);
            break;

          case 'a':
            fputc('\a', stdout);
            break;

          case 'e':
            fputc('\033', stdout);
            break;

          case 'n':
            fputc('\n', stdout);
            break;

          case 'r':
            fputc('\r', stdout);
            break;

          case 't':
            fputc('\t', stdout);
            break;

          case 'v':
            fputc('\v', stdout);
            break;
        }
      }
    } else {
      fputc(cfg->format[i], stdout);
    }
  }
  if (cfg->eol) {
    fputc('\n', stdout);
  }
  fflush(stdout);
}

void print_message(struct mosq_config *cfg, const struct mosquitto_message *message) {
  if (cfg->format) {
    formatted_print(cfg, message);
  } else if (cfg->verbose) {
    if (message->payloadlen) {
      printf("%s ", message->topic);
      write_payload(message->payload, message->payloadlen, false);
      if (cfg->eol) {
        printf("\n");
      }
    } else {
      if (cfg->eol) {
        printf("%s (null)\n", message->topic);
      }
    }
    fflush(stdout);
  } else {
    if (message->payloadlen) {
      write_payload(message->payload, message->payloadlen, false);
      if (cfg->eol) {
        printf("\n");
      }
      fflush(stdout);
    }
  }
}

void signal_handler_func(int signum) {
  if (signum == SIGALRM) {
    process_messages = false;
    mosquitto_disconnect_v5(mosq, MQTT_RC_DISCONNECT_WITH_WILL_MSG, cfg.disconnect_props);
  }
}

void publish_callback_func(struct mosquitto *mosq, void *obj, int mid, int reason_code,
                           const mosquitto_property *properties) {
  UNUSED(obj);
  UNUSED(reason_code);
  UNUSED(properties);

  if (process_messages == false && (mid == last_mid || last_mid == 0)) {
    mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
  }

  printf("publish_callback_func \n");
}

void message_callback_func(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message,
                           const mosquitto_property *properties) {
  int i;
  bool res;

  UNUSED(obj);
  UNUSED(properties);

  if (process_messages == false) return;

  if (cfg.remove_retained && message->retain) {
    mosquitto_publish(mosq, &last_mid, message->topic, 0, NULL, 1, true);
  }

  if (cfg.retained_only && !message->retain && process_messages) {
    process_messages = false;
    if (last_mid == 0) {
      mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
    }
    return;
  }

  if (message->retain && cfg.no_retain) return;
  if (cfg.filter_outs) {
    for (i = 0; i < cfg.filter_out_count; i++) {
      mosquitto_topic_matches_sub(cfg.filter_outs[i], message->topic, &res);
      if (res) return;
    }
  }

  print_message(&cfg, message);

  if (cfg.msg_count > 0) {
    msg_count++;
    if (cfg.msg_count == msg_count) {
      process_messages = false;
      if (last_mid == 0) {
        mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
      }
    }
  }

  printf("message_callback_func \n");
}

void connect_callback_func(struct mosquitto *mosq, void *obj, int result, int flags,
                           const mosquitto_property *properties) {
  int i;

  UNUSED(obj);
  UNUSED(flags);
  UNUSED(properties);

  if (!result) {
    mosquitto_subscribe_multiple(mosq, NULL, cfg.topic_count, cfg.topics, cfg.qos, cfg.sub_opts, cfg.subscribe_props);

    for (i = 0; i < cfg.unsub_topic_count; i++) {
      mosquitto_unsubscribe_v5(mosq, NULL, cfg.unsub_topics[i], cfg.unsubscribe_props);
    }
  } else {
    if (result && !cfg.quiet) {
      if (cfg.protocol_version == MQTT_PROTOCOL_V5) {
        fprintf(stderr, "%s\n", mosquitto_reason_string(result));
      } else {
        fprintf(stderr, "%s\n", mosquitto_connack_string(result));
      }
    }
    mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
  }

  printf("connect_callback_func \n");
}

void subscribe_callback_func(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
  int i;

  UNUSED(obj);

  if (!cfg.quiet) printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
  for (i = 1; i < qos_count; i++) {
    if (!cfg.quiet) printf(", %d", granted_qos[i]);
  }
  if (!cfg.quiet) printf("\n");

  if (cfg.exit_after_sub) {
    mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
  }

  printf("subscribe_callback_func \n");
}

void log_callback_func(struct mosquitto *mosq, void *obj, int level, const char *str) {
  UNUSED(mosq);
  UNUSED(obj);
  UNUSED(level);

  printf("%s\n", str);
}