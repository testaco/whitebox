#include <string.h>
#include <math.h>

#include "whitebox_test.h"

#include "reactor.h"

float diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1e9+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return (temp.tv_sec * 1e9 + temp.tv_nsec) / 1e9;
}

int interval_test_reactor(int sampler_interval_ms) {
    int duration = 1, count;
    struct timespec start, finish, a, b;
    struct reactor *reactor = reactor_new();
    struct io_resource *sampler = reactor_add_resource(reactor,
            "sampler", sampler_source_new, (void*)sampler_interval_ms);
    struct io_resource *sink = reactor_add_resource(reactor,
            "sink", buffer_sink_new, NULL);
    struct buffer_sink *buffer = container_of(sink, struct buffer_sink, r);
    struct io_resource *actor_resource = reactor_add_resource(reactor,
            "actor", actor_new, NULL);
    struct actor *actor = container_of(actor_resource, struct actor, r);
    int timeoff = 0;

    actor_add_open_instr(actor, sink);
    actor_add_open_instr(actor, sampler);
    actor_add_sub_instr(actor, sampler, sink);
    actor_add_instr(actor, "wait", duration, 0, NULL);
    actor_add_instr(actor, "halt", 0, 0, NULL);
    reactor_open_resource(reactor, actor_resource);

    clock_gettime(CLOCK_MONOTONIC, &start);
    reactor_run(reactor);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    printf("time is %f\n", diff(start, finish));
    assert(fabsf(duration - diff(start, finish)) < 0.1);
    printf("count is %d\n", (evbuffer_get_length(sink->buffer)/sizeof(struct timespec)));
    count = evbuffer_get_length(sink->buffer)/sizeof(struct timespec);
    evbuffer_remove(sink->buffer, &a, sizeof(struct timespec));
    while (evbuffer_remove(sink->buffer, &b, sizeof(struct timespec)) > 0) {
        float d = fabsf((float)(diff(a, b) * 1000) - sampler_interval_ms);
        printf("step is %f ", d);
        if (d > ((float)sampler_interval_ms)/1000*2) {
            timeoff++;
            printf("timeoff\n");
        } else {
            printf("\n");
        }
        memcpy(&a, &b, sizeof(struct timespec));
    }
    printf("vs %d\n", buffer->count);
    printf("timeoff %d\n", timeoff);
    assert(timeoff <= 1);
    assert(count > 0);
    assert(count == buffer->count);

    reactor_free(reactor);
    return 0;
}

int test_ptt_sampler(void *data) {
    interval_test_reactor(100);
    interval_test_reactor(50);
    interval_test_reactor(40);
    return 0;
}

int sample_rate_test_reactor(int sampler_interval_ms, int sample_rate,
        int latency_ms) {
    int duration = 10, count;
    struct timespec start, finish, cleanup;
    struct reactor *reactor = reactor_new();
    struct io_resource *sampler = reactor_add_resource(reactor,
            "sampler", sampler_source_new, (void*)sampler_interval_ms);
    int upsampler_rate = sample_rate / (1000 / sampler_interval_ms);
    struct io_resource *upsampler = reactor_add_resource(reactor,
            "upsampler", upsampler_new, (void*)upsampler_rate);
    struct whitebox_sink_config wbsinkconfig = {
        .sample_rate=sample_rate,
        .latency_ms=latency_ms,
    };
    struct io_resource *whitebox_sink = reactor_add_resource(reactor,
            "whitebox_sink", whitebox_sink_new, (void*)&wbsinkconfig);
    struct io_resource *actor_resource = reactor_add_resource(reactor,
            "actor", actor_new, NULL);
    struct actor *actor = container_of(actor_resource, struct actor, r);
    printf("upsampler rate %d\n", upsampler_rate);

    actor_add_open_instr(actor, whitebox_sink);
    actor_add_open_instr(actor, upsampler);
    actor_add_open_instr(actor, sampler);
    actor_add_sub_instr(actor, sampler, upsampler);
    actor_add_sub_instr(actor, upsampler, whitebox_sink);
    actor_add_instr(actor, "wait", duration, 0, NULL);
    actor_add_instr(actor, "halt", 0, 0, NULL);
    reactor_open_resource(reactor, actor_resource);

    clock_gettime(CLOCK_MONOTONIC, &start);
    reactor_run(reactor);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    reactor_free(reactor);
    clock_gettime(CLOCK_MONOTONIC, &cleanup);
    printf("duration %f\n", diff(start, finish));
    assert(abs(duration - diff(start, finish)) < 0.1);
}

int test_whitebox_sink(void *data) {
    sample_rate_test_reactor(200, 200e3, 10);
    //sample_rate_test_reactor(40, 2e6, 100);
    return 0;
}

int test_whitebox_sink_file(void *data) {
    int sample_rate = 200e3;
    int latency_ms = 100;
    int count;
    struct timespec start, finish, cleanup;
    struct reactor *reactor = reactor_new();
    struct whitebox_sink_config wbsinkconfig = {
        .sample_rate=sample_rate,
        .latency_ms=latency_ms,
    };
    struct io_resource *whitebox_sink = reactor_add_resource(reactor,
            "whitebox_sink", whitebox_sink_new, (void*)&wbsinkconfig);
    struct whitebox_sink *wbsink = container_of(whitebox_sink, struct whitebox_sink, r);
    struct io_resource *actor_resource = reactor_add_resource(reactor,
            "actor", actor_new, NULL);
    struct actor *actor = container_of(actor_resource, struct actor, r);

    actor_add_open_instr(actor, whitebox_sink);
    actor_add_io_instr(actor, "writef", whitebox_sink, "/demo1.samples", 0);
    //actor_add_io_instr(actor, "writef", whitebox_sink, "/demo1.samples", 0);
    actor_add_instr(actor, "halt", 0, 0, NULL);
    reactor_open_resource(reactor, actor_resource);

    reactor_run(reactor);

    printf("expecting %d\n", wbsink->bytes);
    assert(wbsink->bytes == whitebox_tx_bytes_total());

    reactor_free(reactor);
    return 0;
}

int main(int argc, char **argv) {
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_ptt_sampler),
        WHITEBOX_TEST(test_whitebox_sink),
        WHITEBOX_TEST(test_whitebox_sink_file),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
