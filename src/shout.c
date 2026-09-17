#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* shout includes */
#include "backend.h"
#include "config.h"
#include "sout.h"

static int running = 1;

int main(int argc, char *argv[]) {
  int rate = DEFAULT_RATE;
  int bit_depth = DEFAULT_BITS;
  int channels = DEFAULT_CHANNELS;

  /* Handling flags */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
      rate = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      bit_depth = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      channels = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-d") == 0) {
      debug = true;
    }
  }

  if (rate <= 0 || (bit_depth != 16 && bit_depth != 32) ||
      (channels != 1 && channels != 2)) {
    soutf("usage: shout [-s socket] [-r rate] [-b 16|32] [-c 1|2] [-d]\n");
    return 1;
  }

  backend_format_t fmt = {
      .rate = rate, .bit_depth = bit_depth, .channels = channels};

  if (debug) {
    soutf("stdin  rate: %d  bits: %d  ch: %d\n", rate, bit_depth, channels);
  }

  if (backend_init(&fmt) != 0) {
    return 1;
  }

  char buffer[BUFSIZE];

  while (running) {
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

    if (n <= 0) {
      break; // EOF o error
    }

    play(buffer, (int)n);
  }

  backend_destroy();
  return 0;
}
