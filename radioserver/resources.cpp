#include <cstring>
#include "resources.h"

source_next source;
sink_cb sink;

int resource_init(struct resource *r) {
    r->data = r->ops->init();
    if (r->data != 0)
        return 0;
    return -1;
}

void resource_close(struct resource *r) {
    r->ops->close(r->data);
}

int resource_descriptors_count(struct resource *r) {
    return r->ops->descriptors_count(r->data);
}

int resource_descriptors(struct resource *r,
        struct pollfd *ufds, int count) {
    return r->ops->descriptors(r->data, ufds, count);
}

void resource_handler(struct resource *r,
        struct pollfd *ufds, int count) {
    r->ops->handler(r->data, ufds, count);
}

int resource_setup(struct resource *r, const char *name, struct resource_ops *ops) {
    strncpy(r->name, name, 255);
    r->ops = ops;
    return resource_init(r);
}

/* ****************************************************** */
/* ** SERVER ******************************************** */
/* ****************************************************** */

struct server {
    int placeholder;
};

void *server_init() {
    struct server *server;
    server = (struct server*)malloc(sizeof(struct server));
    if (server == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }
    if (!server_start(0, port, false)) {
        fprintf(stderr, "Can't start the server.\n");
        free(server);
        return NULL;
    }
    return server;
}

void server_close(void *data) {
    struct server *server = (struct server *)data;
    //server_end();
    free(server);
}

int server_descriptors_count(void *data) {
    struct server *server = (struct server *)data;
    return 0;
}

int server_descriptors(void *data, struct pollfd *ufds, int count) {
    struct server *server = (struct server *)data;
    return 0;
}

void server_handler(void *data, struct pollfd *ufds, int count) {
    
}

struct resource_ops server_ops = {
    server_init,
    server_close,
    server_descriptors_count,
    server_descriptors,
    server_handler,
};

/* ****************************************************** */
/* ** FILE SOURCE *************************************** */
/* ****************************************************** */

struct file_source {
    int fd;
};

void *file_source_init() {
    struct file_source *file_source;
    file_source = (struct file_source*)malloc(sizeof(struct file_source));
    if (file_source == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }
    file_source->fd = open("/mnt/whitebox/gnuradio/audio_50k.samples", O_RDONLY | O_NONBLOCK);
    if (file_source->fd < 0) {
        perror("file_source open");
        free(file_source);
        return NULL;
    }
    poll_start_fd(0, file_source->fd, POLLIN, FILE_SOURCE_FD);
    return file_source;
}

void file_source_close(void *data) {
    struct file_source *file_source = (struct file_source *)data;
    poll_end_fd(file_source->fd);
    close(file_source->fd);
    free(file_source);
}

int file_source_descriptors_count(void *data) {
    struct file_source *file_source = (struct file_source *)data;
    return 1;
}

int file_source_descriptors(void *data, struct pollfd *ufds, int count) {
    struct file_source *file_source = (struct file_source *)data;
    return 0;
}

void file_source_handler(void *data, struct pollfd *ufds, int count) {
    
}

void file_source_to_sink(int fd) {
#if 0
    int16_t audio_data[1024];
    if (sink_space() > 0) {
        int count = read(fd, (void*)audio_data, 1024 * 2);
        if (count < 0) {
            perror("read");
            exit(-1);
        } else if (count == 0) {
            std::cerr << "reset file" << std::endl;
            lseek(fd, 0, SEEK_SET);
        } else {
            for (int i = 0; i < count / 2; ++i) {
                //std::cerr << ' ' << audio_data[i];
                sink(audio_data[i]);
            }
            //std::cerr << std::endl;
        }
    }
#endif
}

struct resource_ops file_source_ops = {
    file_source_init,
    file_source_close,
    file_source_descriptors_count,
    file_source_descriptors,
    file_source_handler,
};

/* ****************************************************** */
/* ** SIGNAL GENERATORS ********************************* */
/* ****************************************************** */

int16_t mute_source(void) {
    return 0;
}

int16_t tone_source(void) {
    static int16_t d = 0;
    int16_t i, q;
    sincos16(fcw1, &phase1, &i, &q);
    // Apply a CW shaping mask
    //if (d < (1 << 14) - 1) //    d += 1;
    //return (int16_t)(((int32_t)d * (int32_t)i) >> 15);
    return i >> 2; //QUAD_PACK(i >> 1, q >> 1);
}

int16_t tone2_source(void) {
    int16_t i1, q1, i2, q2;
    sincos16(fcw1, &phase1, &i1, &q1);
    sincos16(fcw2, &phase2, &i2, &q2);
    //return (i1 / 2 + i2 / 2);
    int16_t i3 = i1 >> 2 + i2 >> 2;
    //int16_t q3 = q1 >> 2 + q2 >> 2;
    return i3 >> 1;
}

int16_t awgn_source(void) {
    int16_t s;
    awgn(&s);
    return s;
}

// Read data from the microphone source, which is stored in the audio
// ring buffer.
int16_t mic_source(void) {
    int16_t sample;
    int end = AUDIO_RING_SIZE - audio_ring_tail;
    int n = (end + audio_ring_head) & (AUDIO_RING_SIZE - 1);
    int data = n < end ? n : end;
    if (data <= 0) {
        sample = 0;
    } else {
        sample = audio_ring[audio_ring_tail];
        audio_ring_tail = (audio_ring_tail + 1) & (AUDIO_RING_SIZE - 2);
    }
    return sample;
}

int16_t modem_source(void) {
    // TODO
    return 0;
}

struct sources_list {
    const char *name;
    source_next next;
};

static const struct sources_list sources_list[] = {
    { "mute", mute_source },
    { "tone", tone_source },
    { "2tone", tone2_source },
    { "noise", awgn_source },
    { "mic", mic_source },
    { "modem", modem_source },
    { NULL, NULL },
};

/* ****************************************************** */
/* ** SIGNAL ANALYZERS ********************************** */
/* ****************************************************** */

int sink_space() {
    int end = AUDIO_RING_SIZE - 1 - audio_ring_head;
    int n = (end + audio_ring_tail) & (AUDIO_RING_SIZE - 1);
    int space = n <= end ? n : end + 1;
    return space;
}

// Write data into the audio ring buffer, to be played on the speakers.
void speaker_sink(int16_t sample) {
    int space = sink_space();
    if (space <= 0) {
        // Audio ring just drops samples; let's catch up!
        return;
    } else {
        audio_ring[audio_ring_head] = sample;
        audio_ring_head = (audio_ring_head + 1) & (AUDIO_RING_SIZE - 1);
    }
}

void null_sink(int16_t sample) {
    // Drop all of the samples.
    return;
}

void modem_sink(int16_t sample) {
    // TODO: do the bit manipulations to set up AFSK.
    // TODO: enqueue the samples; we can't drop these, so return an error.
    // TODO: we can't return an error, yet.
}

struct sinks_list {
    const char *name;
    sink_cb next;
};

static const struct sinks_list sinks_list[] = {
    { "null", null_sink },
    { "speaker", speaker_sink },
    { "modem", modem_sink },
    { NULL, NULL },
};

