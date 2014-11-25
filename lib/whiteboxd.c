#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "reactor.h"
#include "whiteboxd.h"
#include "list.h"

#define FILENAME "/demo1.samples"

int main(int c, char **v) {
    int sample_rate = 200e3;
    int latency_ms = 1000;
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
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    //actor_add_io_instr(actor, "writef", whitebox_sink, FILENAME, 0);
    actor_add_instr(actor, "halt", 0, 0, NULL);
    reactor_open_resource(reactor, actor_resource);

    reactor_run(reactor);

    reactor_free(reactor);
    return 0;
}
