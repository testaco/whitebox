#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Qa Radioserver Source Top Block
# Generated: Tue Nov 10 17:52:30 2015
##################################################

execfile("/home/testa/whitebox/radioserver/gr-radioserver/python/radioserver_source_c.py")
from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import window
from gnuradio.eng_option import eng_option
from gnuradio.gr import firdes
from gnuradio.wxgui import fftsink2
from gnuradio.wxgui import forms
from grc_gnuradio import wxgui as grc_wxgui
from optparse import OptionParser
import wx

class qa_radioserver_source_top_block(grc_wxgui.top_block_gui):

	def __init__(self):
		grc_wxgui.top_block_gui.__init__(self, title="Qa Radioserver Source Top Block")
		_icon_path = "/usr/share/icons/hicolor/32x32/apps/gnuradio-grc.png"
		self.SetIcon(wx.Icon(_icon_path, wx.BITMAP_TYPE_ANY))

		##################################################
		# Variables
		##################################################
		self.samp_rate = samp_rate = 8192
		self.gain = gain = 20
		self.center_freq = center_freq = 146e6

		##################################################
		# Blocks
		##################################################
		_gain_sizer = wx.BoxSizer(wx.VERTICAL)
		self._gain_text_box = forms.text_box(
			parent=self.GetWin(),
			sizer=_gain_sizer,
			value=self.gain,
			callback=self.set_gain,
			label="Gain",
			converter=forms.int_converter(),
			proportion=0,
		)
		self._gain_slider = forms.slider(
			parent=self.GetWin(),
			sizer=_gain_sizer,
			value=self.gain,
			callback=self.set_gain,
			minimum=0,
			maximum=50,
			num_steps=100,
			style=wx.SL_HORIZONTAL,
			cast=int,
			proportion=1,
		)
		self.Add(_gain_sizer)
		_center_freq_sizer = wx.BoxSizer(wx.VERTICAL)
		self._center_freq_text_box = forms.text_box(
			parent=self.GetWin(),
			sizer=_center_freq_sizer,
			value=self.center_freq,
			callback=self.set_center_freq,
			label="Center Frequency",
			converter=forms.float_converter(),
			proportion=0,
		)
		self._center_freq_slider = forms.slider(
			parent=self.GetWin(),
			sizer=_center_freq_sizer,
			value=self.center_freq,
			callback=self.set_center_freq,
			minimum=50e6,
			maximum=1e9,
			num_steps=100,
			style=wx.SL_HORIZONTAL,
			cast=float,
			proportion=1,
		)
		self.Add(_center_freq_sizer)
		self.wxgui_fftsink2_0 = fftsink2.fft_sink_c(
			self.GetWin(),
			baseband_freq=0,
			y_per_div=10,
			y_divs=10,
			ref_level=0,
			ref_scale=2.0,
			sample_rate=samp_rate,
			fft_size=512,
			fft_rate=15,
			average=False,
			avg_alpha=None,
			title="FFT Plot",
			peak_hold=False,
		)
		self.Add(self.wxgui_fftsink2_0.win)
		self.radioserver_source_c_1 = radioserver_source_c("ws://localhost:8080/WebSocket/", samp_rate, center_freq, gain)
		self.blocks_multiply_const_vxx_0 = blocks.multiply_const_vcc((1/2**15.+1j*1/2**15., ))

		##################################################
		# Connections
		##################################################
		self.connect((self.radioserver_source_c_1, 0), (self.blocks_multiply_const_vxx_0, 0))
		self.connect((self.blocks_multiply_const_vxx_0, 0), (self.wxgui_fftsink2_0, 0))


	def get_samp_rate(self):
		return self.samp_rate

	def set_samp_rate(self, samp_rate):
		self.samp_rate = samp_rate
		self.wxgui_fftsink2_0.set_sample_rate(self.samp_rate)

	def get_gain(self):
		return self.gain

	def set_gain(self, gain):
		self.gain = gain
		self._gain_slider.set_value(self.gain)
		self._gain_text_box.set_value(self.gain)

	def get_center_freq(self):
		return self.center_freq

	def set_center_freq(self, center_freq):
		self.center_freq = center_freq
		self._center_freq_slider.set_value(self.center_freq)
		self._center_freq_text_box.set_value(self.center_freq)

if __name__ == '__main__':
	parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
	(options, args) = parser.parse_args()
	tb = qa_radioserver_source_top_block()
	tb.Run(True)

