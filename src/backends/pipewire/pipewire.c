#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../backend.h"
#include "../../sout.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/utils/hook.h>

#define RING_SIZE (1 << 20) /* 1 MiB, and is a power of two */

static struct pw_main_loop *loop;
static struct pw_context *context;
static struct pw_core *core;
static struct pw_stream *stream;
static struct spa_hook stream_listener;
static pthread_t pw_thread;
static int pw_thread_started;

static size_t frame_size; /* bytes per PCM frame */

/* single producer, single consumer */
static unsigned char ring[RING_SIZE];
static atomic_uint head;
static atomic_uint tail;

static size_t ring_used(void) {
  unsigned h = atomic_load_explicit(&head, memory_order_acquire);
  unsigned t = atomic_load_explicit(&tail, memory_order_acquire);
  return (h - t) & (RING_SIZE - 1);
}

size_t ring_free(void) { return RING_SIZE - 1 - ring_used(); }

void tiny_sleep(void) {
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 200000}; /* 200us */
  nanosleep(&ts, NULL);
}

int play(const void *pcm, int length) {
  const unsigned char *src = (const unsigned char *)pcm;
  int total = 0;

  while (total < length) {
    size_t chunk = ring_free();
    if (chunk > (size_t)(length - total))
      chunk = (size_t)(length - total);

    if (chunk == 0) { /* wait for the consumer to free space */
      tiny_sleep();
      continue;
    }

    unsigned h = atomic_load_explicit(&head, memory_order_relaxed);
    size_t first = chunk;
    if (first > RING_SIZE - h)
      first = RING_SIZE - h;

    memcpy(ring + h, src + total, first);
    memcpy(ring, src + total + first, chunk - first);

    /* publish the written bytes */
    atomic_store_explicit(&head, (h + chunk) & (RING_SIZE - 1),
                          memory_order_release);

    total += (int)chunk;
  }

  return length;
}

void on_process(void *data) {
  (void)data;

  struct pw_buffer *buf = pw_stream_dequeue_buffer(stream);
  if (!buf)
    return;

  struct spa_buffer *spa = buf->buffer;
  struct spa_data *d = spa->datas;
  size_t max = d->maxsize;

  size_t n = ring_used();
  if (n > max)
    n = max;
  n -= n % frame_size; /* all frames only */

  unsigned t = atomic_load_explicit(&tail, memory_order_relaxed);
  size_t first = n;
  if (first > RING_SIZE - t)
    first = RING_SIZE - t;

  memcpy(d->data, ring + t, first);
  memcpy((char *)d->data + first, ring, n - first);

  atomic_store_explicit(&tail, (t + n) & (RING_SIZE - 1), memory_order_release);

  /* silence-pad any undrrun to avoid any weird sound */
  if (n < max)
    memset((char *)d->data + n, 0, max - n);

  d->chunk->offset = 0;
  d->chunk->size = max;
  d->chunk->stride = frame_size;

  pw_stream_queue_buffer(stream, buf);
}

void on_state_changed(void *data, enum pw_stream_state old,
                      enum pw_stream_state state, const char *error) {
  (void)data;
  (void)old;
  if (state == PW_STREAM_STATE_ERROR && error)
    soutf("pipewire error: %s\n", error);
}

struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_state_changed,
    .process = on_process,
};

void *loop_thread(void *arg) {
  (void)arg;
  pw_main_loop_run(loop);
  return NULL;
}

int backend_init(const backend_format_t *fmt) {
  int res = -1;
  char chstr[8];

  pw_init(NULL, NULL);

  loop = pw_main_loop_new(NULL);
  if (!loop) {
    soutf("pw_main_loop_new failed\n");
    goto fail;
  }

  context = pw_context_new(pw_main_loop_get_loop(loop), NULL, 0);
  if (!context) {
    soutf("pw_context_new failed\n");
    goto fail_loop;
  }

  core = pw_context_connect(context, NULL, 0);
  if (!core) {
    soutf("pw_context_connect failed\n");
    goto fail_context;
  }

  struct pw_properties *props = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback",
      PW_KEY_MEDIA_ROLE, "Music", PW_KEY_APP_NAME, "shout", PW_KEY_NODE_NAME,
      "shout", PW_KEY_NODE_DESCRIPTION, "shout (audio daemon)", NULL);
  if (!props) {
    soutf("pw_properties_new failed\n");
    goto fail_loop;
  }

  snprintf(chstr, sizeof chstr, "%d", fmt->channels);
  pw_properties_set(props, PW_KEY_AUDIO_CHANNELS, chstr);

  stream = pw_stream_new(core, "shout", props);
  if (!stream) {
    soutf("pw_stream_new failed\n");
    goto fail_loop;
  }

  frame_size = (size_t)fmt->channels * (fmt->bit_depth == 32 ? 4 : 2);

  struct spa_audio_info_raw info = {0};
  info.format =
      fmt->bit_depth == 32 ? SPA_AUDIO_FORMAT_S32 : SPA_AUDIO_FORMAT_S16;
  info.rate = (uint32_t)fmt->rate;
  info.channels = (uint32_t)fmt->channels;

  uint8_t pod_data[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_data, sizeof pod_data);
  const struct spa_pod *params[1];
  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

  pw_stream_add_listener(stream, &stream_listener, &stream_events, NULL);

  res = pw_stream_connect(
      stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS, params, 1);
  if (res < 0) {
    soutf("pw_stream_connect failed: %s\n", strerror(-res));
    goto fail_stream;
  }

  if (pthread_create(&pw_thread, NULL, loop_thread, NULL) != 0) {
    soutf("pthread_create failed\n");
    goto fail_stream;
  }
  pw_thread_started = 1;

  return 0;

fail_stream:
  pw_stream_destroy(stream);
  stream = NULL;
fail_context:
  pw_context_destroy(context);
  context = NULL;
fail_loop:
  pw_main_loop_destroy(loop);
  loop = NULL;
fail:
  pw_deinit();
  return res;
}

void backend_destroy(void) {
  if (loop)
    pw_main_loop_quit(loop);
  if (pw_thread_started)
    pthread_join(pw_thread, NULL);
  if (stream) {
    pw_stream_destroy(stream);
    stream = NULL;
  }
  if (context) {
    pw_context_destroy(context);
    context = NULL;
  }
  if (loop) {
    pw_main_loop_destroy(loop);
    loop = NULL;
  }
  pw_deinit();
}
