#ifndef backend
#define backend

/* This just declares function, compile the backend you want and link and thats
 * it*/

typedef struct {
  int rate;
  int bit_depth;
  int channels;
} backend_format_t;

int backend_init(const backend_format_t *format);
int play(const void *pcm, int length);
void backend_destroy(void);

#endif
