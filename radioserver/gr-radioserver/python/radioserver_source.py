#!/usr/bin/env python
# 
# Copyright 2015 <+YOU OR YOUR COMPANY+>.
# 
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
# 

import numpy
from gnuradio import gr

from radioclient import radio_client

class radioserver_source(gr.sync_block):
    """
    Connects to a radioserver, enters receive mode with a test pattern.
    """
    def __init__(self, url, samp_rate, center_frequency, gain):
        gr.sync_block.__init__(self,
            name="radioserver_source",
            in_sig=None,
            out_sig=[numpy.int16])

        self.client = radio_client(url, samp_rate, center_frequency, gain)

        ##################################################
        # Variables
        ##################################################
        self.url = url
        self.samp_rate = samp_rate
        self.center_frequency = center_frequency
        self.gain = gain

    def start(self):
        self.client.start()
        self.client.receive()

    def stop(self):
        self.client.stop()

    def work(self, input_items, output_items):
        out = output_items[0]
        return self.client.receive_ishort(out)

    def get_url(self):
        return self.url

    def set_url(self, url):
        self.url = url
        self.radioclient.set_url(url)

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.radioclient.set_samp_rate(samp_rate)

    def get_gain(self):
        return self.gain

    def set_gain(self, gain):
        self.gain = gain
        self.radioclient.set_gain(center_gain)

    def get_center_frequency(self):
        return self.center_frequency

    def set_center_frequency(self, center_frequency):
        self.center_frequency = center_frequency
        self.radioclient.set_center_frequency(center_frequency)
