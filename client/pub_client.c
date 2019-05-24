#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "client_shared.h"
#include "pub_utils.h"

int main(int argc, char *argv[]) {
  struct mosquitto *mosq = NULL;
  mosq_retcode_t ret;

  mosquitto_lib_init();

  if (pub_shared_init()) {
    return EXIT_FAILURE;
  }

  init_config(&cfg, CLIENT_PUB);

  // set the configures and message for testing
  cfg.host = strdup(HOST);
  if (cfg_add_topic(&cfg, CLIENT_PUB, TOPIC)) {
    return EXIT_FAILURE;
  }
  if (cfg.pub_mode != MSGMODE_NONE) {
    fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
    return EXIT_FAILURE;
  } else {
    cfg.message = strdup(MESSAGE);
    cfg.msglen = strlen(cfg.message);
    cfg.pub_mode = MSGMODE_CMD;
  }

  if (client_id_generate(&cfg)) {
    goto cleanup;
  }

  init_check_error(&cfg, CLIENT_PUB);

  mosq = mosquitto_new(cfg.id, true, NULL);
  if (!mosq) {
    switch (errno) {
      case ENOMEM:
        if (!cfg.quiet) fprintf(stderr, "Error: Out of memory.\n");
        break;
      case EINVAL:
        if (!cfg.quiet) fprintf(stderr, "Error: Invalid id.\n");
        break;
    }
    goto cleanup;
  }

  if (client_opts_set(mosq, &cfg)) {
    goto cleanup;
  }

  if (cfg.debug) {
    mosquitto_log_callback_set(mosq, log_callback_func);
  }
  mosquitto_connect_v5_callback_set(mosq, connect_callback_func);
  mosquitto_disconnect_v5_callback_set(mosq, disconnect_callback_func);
  mosquitto_publish_v5_callback_set(mosq, publish_callback_func);

  ret = client_connect(mosq, &cfg);
  if (ret) {
    goto cleanup;
  }

  ret = publish_loop(mosq);

  if (cfg.message && cfg.pub_mode == MSGMODE_FILE) {
    free(cfg.message);
  }
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  client_config_cleanup(&cfg);
  pub_shared_cleanup();

  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
  }
  return ret;

cleanup:
  mosquitto_lib_cleanup();
  client_config_cleanup(&cfg);
  pub_shared_cleanup();
  return EXIT_FAILURE;
}
