#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: RadioServer Complex Source
# Generated: Sat Oct 31 10:56:27 2015
##################################################

from gnuradio import blocks
from gnuradio import gr
from gnuradio.gr import firdes
from grc_gnuradio import blks2 as grc_blks2

class radioserver_source_c(gr.hier_block2):

	def __init__(self):
		gr.hier_block2.__init__(
			self, "RadioServer Complex Source",
			gr.io_signature(0, 0, 0),
			gr.io_signature(1, 1, gr.sizeof_gr_complex*1),
		)

		##################################################
		# Variables
		##################################################
		self.uri = uri = "ws://localhost:8080/WebSocket/"
		self.samp_rate = samp_rate = 8192
		self.gain = gain = 0
		self.center_frequency = center_frequency = 0

		##################################################
		# Blocks
		##################################################
		self.radioserver_client_0 =  radioserver_source(
            uri)
            #grc_blks2.tcp_source(
            #	itemsize=gr.sizeof_short*1,
            #	addr=uri,
            #	port=0,
            #	server=False,
            #)
		self.blocks_interleaved_short_to_complex_0 = blocks.interleaved_short_to_complex()

		##################################################
		# Connections
		##################################################
		self.connect((self.radioserver_client_0, 0), (self.blocks_interleaved_short_to_complex_0, 0))
		self.connect((self.blocks_interleaved_short_to_complex_0, 0), (self, 0))


	def get_uri(self):
		return self.uri

	def set_uri(self, uri):
		self.uri = uri

	def get_samp_rate(self):
		return self.samp_rate

	def set_samp_rate(self, samp_rate):
		self.samp_rate = samp_rate

	def get_gain(self):
		return self.gain

	def set_gain(self, gain):
		self.gain = gain

	def get_center_frequency(self):
		return self.center_frequency

	def set_center_frequency(self, center_frequency):
		self.center_frequency = center_frequency


