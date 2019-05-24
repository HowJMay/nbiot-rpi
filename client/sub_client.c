#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_shared.h"
#include "sub_utils.h"

int main(int argc, char *argv[]) {
  mosq_retcode_t ret;
  struct sigaction sigact;

  init_config(&cfg, CLIENT_SUB);
  mosquitto_lib_init();

  // set the configures and message for testing
  cfg.host = strdup(HOST);
  if (cfg_add_topic(&cfg, CLIENT_SUB, TOPIC)) {
    return EXIT_FAILURE;
  }

  if (cfg.no_retain && cfg.retained_only) {
    fprintf(stderr, "\nError: Combining '-R' and '--retained-only' makes no sense.\n");
    goto cleanup;
  }

  if (client_id_generate(&cfg)) {
    goto cleanup;
  }

  mosq = mosquitto_new(cfg.id, cfg.clean_session, &cfg);
  if (!mosq) {
    switch (errno) {
      case ENOMEM:
        if (!cfg.quiet) fprintf(stderr, "Error: Out of memory.\n");
        break;
      case EINVAL:
        if (!cfg.quiet) fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
        break;
    }
    goto cleanup;
  }

  if (client_opts_set(mosq, &cfg)) {
    goto cleanup;
  }

  if (cfg.debug) {
    mosquitto_log_callback_set(mosq, log_callback_func);
    mosquitto_subscribe_callback_set(mosq, subscribe_callback_func);
  }
  mosquitto_connect_v5_callback_set(mosq, connect_callback_func);
  mosquitto_message_v5_callback_set(mosq, message_callback_func);

  ret = client_connect(mosq, &cfg);
  if (ret) {
    goto cleanup;
  }

  sigact.sa_handler = signal_handler_func;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;

  if (sigaction(SIGALRM, &sigact, NULL) == -1) {
    perror("sigaction");
    goto cleanup;
  }

  if (cfg.timeout) {
    alarm(cfg.timeout);
  }

  ret = mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();

  if (cfg.msg_count > 0 && ret == MOSQ_ERR_NO_CONN) {
    ret = MOSQ_ERR_SUCCESS;
  }
  client_config_cleanup(&cfg);
  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
  }
  return ret;

cleanup:
  mosquitto_lib_cleanup();
  client_config_cleanup(&cfg);
  return 1;
}
