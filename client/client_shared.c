#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_shared.h"

#ifdef WITH_SOCKS
static int mosquitto__parse_socks_url(struct mosq_config *cfg, char *url);
#endif

void init_config(struct mosq_config *cfg, int pub_or_sub) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->port = -1;
  cfg->max_inflight = 20;
  cfg->keepalive = 60;
  cfg->clean_session = true;
  cfg->eol = true;
  cfg->repeat_count = 1;
  cfg->repeat_delay.tv_sec = 0;
  cfg->repeat_delay.tv_usec = 0;
  if (pub_or_sub == CLIENT_RR) {
    cfg->protocol_version = MQTT_PROTOCOL_V5;
    cfg->msg_count = 1;
  } else {
    cfg->protocol_version = MQTT_PROTOCOL_V311;
  }
}

void client_config_cleanup(struct mosq_config *cfg) {
  int i;
  free(cfg->id);
  free(cfg->id_prefix);
  free(cfg->host);
  free(cfg->file_input);
  free(cfg->message);
  free(cfg->topic);
  free(cfg->bind_address);
  free(cfg->username);
  free(cfg->password);
  free(cfg->will_topic);
  free(cfg->will_payload);
  free(cfg->format);
  free(cfg->response_topic);
#ifdef WITH_TLS
  free(cfg->cafile);
  free(cfg->capath);
  free(cfg->certfile);
  free(cfg->keyfile);
  free(cfg->ciphers);
  free(cfg->tls_alpn);
  free(cfg->tls_version);
  free(cfg->tls_engine);
  free(cfg->tls_engine_kpass_sha1);
  free(cfg->keyform);
#ifdef FINAL_WITH_TLS_PSK
  free(cfg->psk);
  free(cfg->psk_identity);
#endif
#endif
  if (cfg->topics) {
    for (i = 0; i < cfg->topic_count; i++) {
      free(cfg->topics[i]);
    }
    free(cfg->topics);
  }
  if (cfg->filter_outs) {
    for (i = 0; i < cfg->filter_out_count; i++) {
      free(cfg->filter_outs[i]);
    }
    free(cfg->filter_outs);
  }
  if (cfg->unsub_topics) {
    for (i = 0; i < cfg->unsub_topic_count; i++) {
      free(cfg->unsub_topics[i]);
    }
    free(cfg->unsub_topics);
  }
#ifdef WITH_SOCKS
  free(cfg->socks5_host);
  free(cfg->socks5_username);
  free(cfg->socks5_password);
#endif
  mosquitto_property_free_all(&cfg->connect_props);
  mosquitto_property_free_all(&cfg->publish_props);
  mosquitto_property_free_all(&cfg->subscribe_props);
  mosquitto_property_free_all(&cfg->unsubscribe_props);
  mosquitto_property_free_all(&cfg->disconnect_props);
  mosquitto_property_free_all(&cfg->will_props);
}

int cfg_add_topic(struct mosq_config *cfg, int type, char *topic) {
  if (mosquitto_validate_utf8(topic, strlen(topic))) {
    fprintf(stderr, "Error: Malformed UTF-8 in topic argument.\n\n");
    return EXIT_FAILURE;
  }
  if (type == CLIENT_PUB || type == CLIENT_RR) {
    if (mosquitto_pub_topic_check(topic) == MOSQ_ERR_INVAL) {
      fprintf(stderr, "Error: Invalid publish topic '%s', does it contain '+' or '#'?\n", topic);
      return EXIT_FAILURE;
    }
    cfg->topic = strdup(topic);
  } else if (type == CLIENT_RESPONSE_TOPIC) {
    if (mosquitto_pub_topic_check(topic) == MOSQ_ERR_INVAL) {
      fprintf(stderr, "Error: Invalid response topic '%s', does it contain '+' or '#'?\n", topic);
      return EXIT_FAILURE;
    }
    cfg->response_topic = strdup(topic);
  } else {
    if (mosquitto_sub_topic_check(topic) == MOSQ_ERR_INVAL) {
      fprintf(stderr, "Error: Invalid subscription topic '%s', are all '+' and '#' wildcards correct?\n", topic);
      return EXIT_FAILURE;
    }
    cfg->topic_count++;
    cfg->topics = realloc(cfg->topics, cfg->topic_count * sizeof(char *));
    if (!cfg->topics) {
      fprintf(stderr, "Error: Out of memory.\n");
      return EXIT_FAILURE;
    }
    cfg->topics[cfg->topic_count - 1] = strdup(topic);
  }
  return 0;
}

int client_opts_set(struct mosquitto *mosq, struct mosq_config *cfg) {
#if defined(WITH_TLS) || defined(WITH_SOCKS)
  int rc;
#endif

  mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, cfg->protocol_version);

  if (cfg->will_topic && mosquitto_will_set_v5(mosq, cfg->will_topic, cfg->will_payloadlen, cfg->will_payload,
                                               cfg->will_qos, cfg->will_retain, cfg->will_props)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting will.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
  cfg->will_props = NULL;

  if (cfg->username && mosquitto_username_pw_set(mosq, cfg->username, cfg->password)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting username and password.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
#ifdef WITH_TLS
  if (cfg->cafile || cfg->capath) {
    rc = mosquitto_tls_set(mosq, cfg->cafile, cfg->capath, cfg->certfile, cfg->keyfile, NULL);
    if (rc) {
      if (rc == MOSQ_ERR_INVAL) {
        if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS options: File not found.\n");
      } else {
        if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS options: %s.\n", mosquitto_strerror(rc));
      }
      mosquitto_lib_cleanup();
      return EXIT_FAILURE;
    }
  }
  if (cfg->insecure && mosquitto_tls_insecure_set(mosq, true)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS insecure option.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
  if (cfg->tls_engine && mosquitto_string_option(mosq, MOSQ_OPT_TLS_ENGINE, cfg->tls_engine)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS engine, is %s a valid engine?\n", cfg->tls_engine);
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
  if (cfg->keyform && mosquitto_string_option(mosq, MOSQ_OPT_TLS_KEYFORM, cfg->keyform)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting key form, it must be one of 'pem' or 'engine'.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
  if (cfg->tls_engine_kpass_sha1 &&
      mosquitto_string_option(mosq, MOSQ_OPT_TLS_ENGINE_KPASS_SHA1, cfg->tls_engine_kpass_sha1)) {
    if (!cfg->quiet)
      fprintf(stderr, "Error: Problem setting TLS engine key pass sha, is it a 40 character hex string?\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
  if (cfg->tls_alpn && mosquitto_string_option(mosq, MOSQ_OPT_TLS_ALPN, cfg->tls_alpn)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS ALPN protocol.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
#ifdef FINAL_WITH_TLS_PSK
  if (cfg->psk && mosquitto_tls_psk_set(mosq, cfg->psk, cfg->psk_identity, NULL)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS-PSK options.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
#endif
  if ((cfg->tls_version || cfg->ciphers) && mosquitto_tls_opts_set(mosq, 1, cfg->tls_version, cfg->ciphers)) {
    if (!cfg->quiet) fprintf(stderr, "Error: Problem setting TLS options, check the options are valid.\n");
    mosquitto_lib_cleanup();
    return EXIT_FAILURE;
  }
#endif
  mosquitto_max_inflight_messages_set(mosq, cfg->max_inflight);
#ifdef WITH_SOCKS
  if (cfg->socks5_host) {
    rc = mosquitto_socks5_set(mosq, cfg->socks5_host, cfg->socks5_port, cfg->socks5_username, cfg->socks5_password);
    if (rc) {
      mosquitto_lib_cleanup();
      return rc;
    }
  }
#endif
  return MOSQ_ERR_SUCCESS;
}

int client_id_generate(struct mosq_config *cfg) {
  if (cfg->id_prefix) {
    cfg->id = malloc(strlen(cfg->id_prefix) + 10);
    if (!cfg->id) {
      if (!cfg->quiet) fprintf(stderr, "Error: Out of memory.\n");
      mosquitto_lib_cleanup();
      return EXIT_FAILURE;
    }
    snprintf(cfg->id, strlen(cfg->id_prefix) + 10, "%s%d", cfg->id_prefix, getpid());
  }
  return MOSQ_ERR_SUCCESS;
}

int client_connect(struct mosquitto *mosq, struct mosq_config *cfg) {
  char *err;
  int rc;
  int port;

  if (cfg->port < 0) {
#ifdef WITH_TLS
    if (cfg->cafile || cfg->capath
#ifdef FINAL_WITH_TLS_PSK
        || cfg->psk
#endif
    ) {
      port = 8883;
    } else
#endif
    {
      port = 1883;
    }
  } else {
    port = cfg->port;
  }

#ifdef WITH_SRV
  if (cfg->use_srv) {
    rc = mosquitto_connect_srv(mosq, cfg->host, cfg->keepalive, cfg->bind_address);
  } else {
    rc = mosquitto_connect_bind_v5(mosq, cfg->host, port, cfg->keepalive, cfg->bind_address, cfg->connect_props);
  }
#else
  rc = mosquitto_connect_bind_v5(mosq, cfg->host, port, cfg->keepalive, cfg->bind_address, cfg->connect_props);
#endif
  if (rc > 0) {
    if (!cfg->quiet) {
      if (rc == MOSQ_ERR_ERRNO) {
        err = strerror(errno);
        fprintf(stderr, "Error: %s\n", err);
      } else {
        fprintf(stderr, "Unable to connect (%s).\n", mosquitto_strerror(rc));
      }
    }
    mosquitto_lib_cleanup();
    return rc;
  }
  return MOSQ_ERR_SUCCESS;
}

#ifdef WITH_SOCKS
/* Convert %25 -> %, %3a, %3A -> :, %40 -> @ */
static int mosquitto__urldecode(char *str) {
  int i, j;
  int len;
  if (!str) return 0;

  if (!strchr(str, '%')) return 0;

  len = strlen(str);
  for (i = 0; i < len; i++) {
    if (str[i] == '%') {
      if (i + 2 >= len) {
        return EXIT_FAILURE;
      }
      if (str[i + 1] == '2' && str[i + 2] == '5') {
        str[i] = '%';
        len -= 2;
        for (j = i + 1; j < len; j++) {
          str[j] = str[j + 2];
        }
        str[j] = '\0';
      } else if (str[i + 1] == '3' && (str[i + 2] == 'A' || str[i + 2] == 'a')) {
        str[i] = ':';
        len -= 2;
        for (j = i + 1; j < len; j++) {
          str[j] = str[j + 2];
        }
        str[j] = '\0';
      } else if (str[i + 1] == '4' && str[i + 2] == '0') {
        str[i] = ':';
        len -= 2;
        for (j = i + 1; j < len; j++) {
          str[j] = str[j + 2];
        }
        str[j] = '\0';
      } else {
        return EXIT_FAILURE;
      }
    }
  }
  return 0;
}

static int mosquitto__parse_socks_url(struct mosq_config *cfg, char *url) {
  char *str;
  size_t i;
  char *username = NULL, *password = NULL, *host = NULL, *port = NULL;
  char *username_or_host = NULL;
  size_t start;
  size_t len;
  bool have_auth = false;
  int port_int;

  if (!strncmp(url, "socks5h://", strlen("socks5h://"))) {
    str = url + strlen("socks5h://");
  } else {
    fprintf(stderr, "Error: Unsupported proxy protocol: %s\n", url);
    return EXIT_FAILURE;
  }

  // socks5h://username:password@host:1883
  // socks5h://username:password@host
  // socks5h://username@host:1883
  // socks5h://username@host
  // socks5h://host:1883
  // socks5h://host

  start = 0;
  for (i = 0; i < strlen(str); i++) {
    if (str[i] == ':') {
      if (i == start) {
        goto cleanup;
      }
      if (have_auth) {
        /* Have already seen a @ , so this must be of form
         * socks5h://username[:password]@host:port */
        if (host) {
          /* Already seen a host, must be malformed. */
          goto cleanup;
        }
        len = i - start;
        host = malloc(len + 1);
        if (!host) {
          fprintf(stderr, "Error: Out of memory.\n");
          goto cleanup;
        }
        memcpy(host, &(str[start]), len);
        host[len] = '\0';
        start = i + 1;
      } else if (!username_or_host) {
        /* Haven't seen a @ before, so must be of form
         * socks5h://host:port or
         * socks5h://username:password@host[:port] */
        len = i - start;
        username_or_host = malloc(len + 1);
        if (!username_or_host) {
          fprintf(stderr, "Error: Out of memory.\n");
          goto cleanup;
        }
        memcpy(username_or_host, &(str[start]), len);
        username_or_host[len] = '\0';
        start = i + 1;
      }
    } else if (str[i] == '@') {
      if (i == start) {
        goto cleanup;
      }
      have_auth = true;
      if (username_or_host) {
        /* Must be of form socks5h://username:password@... */
        username = username_or_host;
        username_or_host = NULL;

        len = i - start;
        password = malloc(len + 1);
        if (!password) {
          fprintf(stderr, "Error: Out of memory.\n");
          goto cleanup;
        }
        memcpy(password, &(str[start]), len);
        password[len] = '\0';
        start = i + 1;
      } else {
        /* Haven't seen a : yet, so must be of form
         * socks5h://username@... */
        if (username) {
          /* Already got a username, must be malformed. */
          goto cleanup;
        }
        len = i - start;
        username = malloc(len + 1);
        if (!username) {
          fprintf(stderr, "Error: Out of memory.\n");
          goto cleanup;
        }
        memcpy(username, &(str[start]), len);
        username[len] = '\0';
        start = i + 1;
      }
    }
  }

  /* Deal with remainder */
  if (i > start) {
    len = i - start;
    if (host) {
      /* Have already seen a @ , so this must be of form
       * socks5h://username[:password]@host:port */
      port = malloc(len + 1);
      if (!port) {
        fprintf(stderr, "Error: Out of memory.\n");
        goto cleanup;
      }
      memcpy(port, &(str[start]), len);
      port[len] = '\0';
    } else if (username_or_host) {
      /* Haven't seen a @ before, so must be of form
       * socks5h://host:port */
      host = username_or_host;
      username_or_host = NULL;
      port = malloc(len + 1);
      if (!port) {
        fprintf(stderr, "Error: Out of memory.\n");
        goto cleanup;
      }
      memcpy(port, &(str[start]), len);
      port[len] = '\0';
    } else {
      host = malloc(len + 1);
      if (!host) {
        fprintf(stderr, "Error: Out of memory.\n");
        goto cleanup;
      }
      memcpy(host, &(str[start]), len);
      host[len] = '\0';
    }
  }

  if (!host) {
    fprintf(stderr, "Error: Invalid proxy.\n");
    goto cleanup;
  }

  if (mosquitto__urldecode(username)) {
    goto cleanup;
  }
  if (mosquitto__urldecode(password)) {
    goto cleanup;
  }
  if (port) {
    port_int = atoi(port);
    if (port_int < 1 || port_int > 65535) {
      fprintf(stderr, "Error: Invalid proxy port %d\n", port_int);
      goto cleanup;
    }
    free(port);
  } else {
    port_int = 1080;
  }

  cfg->socks5_username = username;
  cfg->socks5_password = password;
  cfg->socks5_host = host;
  cfg->socks5_port = port_int;

  return 0;
cleanup:
  if (username_or_host) free(username_or_host);
  if (username) free(username);
  if (password) free(password);
  if (host) free(host);
  if (port) free(port);
  return EXIT_FAILURE;
}

#endif
