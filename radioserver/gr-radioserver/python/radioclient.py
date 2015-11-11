import json
import struct

from websocket import create_connection

class radio_client(object):
    def __init__(self, url, samp_rate, center_frequency, gain):
        self.url = url
        self.samp_rate = samp_rate
        self.center_frequency = center_frequency
        self.gain = gain

        self._cur_data = None
        self._cur_index = 0

    def start(self):
        self.ws = create_connection(self.url,
            header=['Sec-WebSocket-Protocol: radio-server-1',])
        self.set_status(net_sample_rate=self.samp_rate,
            center_frequency=self.center_frequency,
            gain=self.gain,
        )

    def stop(self):
        self.ws.close()

    def send_cmd(self, cmd, **kwargs):
        cmd = { 'command': cmd }
        cmd.update(kwargs);
        print 'Sending', cmd
        self.ws.send(json.dumps(cmd))

    def receive(self):
        self.send_cmd('receive')

    def transmit(self):
        self.send_cmd('transmit')

    def get_status(self):
        # TODO
        pass

    def set_status(self, **kwargs):
        self.send_cmd('set', **kwargs)

    #def transmit_ishort(self, typ, data):
    #    data = [ord(i) for i in data]
    #    frame = struct.pack('I' + 'h' * len(data), typ, *data)
    #    print "transmit_hh", len(frame)
    #    self.ws.send_binary(frame)

    #def transmit_complex(self, x):
    #    x_scaled = x * 2**14
    #    x_fixed = [(int(i.imag), int(i.real)) for i in x_scaled]
    #    x_packed = [val for sublist in x_fixed for val in sublist]
    #    print x_packed
    #    print len(x_packed)
    #    data = struct.pack('h' * len(x_packed), *x_packed)
    #    print data
    #    return self.transmit_hh(2, data)

    def receive_ishort(self, out):
        if not self._cur_data:
            self._cur_data = self.ws.recv()
            # Convert from bytes to shorts
            if len(self._cur_data) <= 4:
                self._cur_data = None
                return 0
            assert len(self._cur_data) % 4 == 0
            self._cur_data = struct.unpack('hh' * (len(self._cur_data) / 4),
                                           self._cur_data)
            self._cur_data = self._cur_data[2:]
            print "Getting some data...", len(self._cur_data)
            print self._cur_data

            self._cur_index = 0
        length = min(len(out), len(self._cur_data) - self._cur_index)
        print "Working with", length, self._cur_index
        for i in xrange(length):
            #print 'in_index=%d out_index=%d data=%d' % (self._cur_index, i, self._cur_data[self._cur_index])
            out[i] = self._cur_data[self._cur_index]
            self._cur_index += 1
        if self._cur_index >= len(self._cur_data):
            self._cur_data = None
        return length
