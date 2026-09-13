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

static void on_signal(int s) {
  (void)s;
  running = 0;
}

int main(int argc, char *argv[]) {
  const char *socket_path = DEFAULT_SOCKET;
  int rate = DEFAULT_RATE;
  int bit_depth = DEFAULT_BITS;
  int channels = DEFAULT_CHANNELS;

  /* Handling flags */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      socket_path = argv[++i];
    } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
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
    sout("usage: shout [-s socket] [-r rate] [-b 16|32] [-c 1|2] [-d]\n");
    return 1;
  }

  backend_format_t fmt = {rate, bit_depth, channels};

  /* This is if by some weird reason there was something of a past run */
  unlink(socket_path);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0); /* The PCM Byte stream */
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, socket_path, sizeof addr.sun_path - 1);

  if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
      listen(fd, 1) < 0) {
    sout("failed to set up socket\n");
    return 1;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  if (debug) {
    sout("socket: %s  rate: %d  bits: %d  ch: %d\n", socket_path, rate,
         bit_depth, channels);
  }

  if (backend_init(&fmt) != 0) {
    return 1;
  }

  char buffer[BUFSIZE];

  while (running) {
    int c = accept(fd, NULL, NULL);
    if (c < 0) {
      continue;
    }

    while (running) {
      ssize_t n = read(c, buffer, sizeof buffer);
      if (n <= 0) {
        break;
      }

      play(buffer, (int)n); /* Here is the magic */
    }
    close(c);
  }

  backend_destroy();
  close(fd);
  unlink(socket_path);
  return 0;
}
