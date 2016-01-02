#!/usr/bin/env python
##################################################
# Gnuradio Python Flow Graph
# Title: Qa Radioserver Source Top Block
# Generated: Sat Jan  2 10:22:59 2016
##################################################

execfile("/home/testa/whitebox/radioserver/gr-radioserver/python/radioserver_source_c.py")
from PyQt4 import Qt
from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio.eng_option import eng_option
from gnuradio.gr import firdes
from gnuradio.qtgui import qtgui
from optparse import OptionParser
import PyQt4.Qwt5 as Qwt
import sip
import sys

class qa_radioserver_source_top_block(gr.top_block, Qt.QWidget):

	def __init__(self):
		gr.top_block.__init__(self, "Qa Radioserver Source Top Block")
		Qt.QWidget.__init__(self)
		self.setWindowTitle("Qa Radioserver Source Top Block")
		self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
		self.top_scroll_layout = Qt.QVBoxLayout()
		self.setLayout(self.top_scroll_layout)
		self.top_scroll = Qt.QScrollArea()
		self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
		self.top_scroll_layout.addWidget(self.top_scroll)
		self.top_scroll.setWidgetResizable(True)
		self.top_widget = Qt.QWidget()
		self.top_scroll.setWidget(self.top_widget)
		self.top_layout = Qt.QVBoxLayout(self.top_widget)
		self.top_grid_layout = Qt.QGridLayout()
		self.top_layout.addLayout(self.top_grid_layout)


		##################################################
		# Variables
		##################################################
		self.samp_rate = samp_rate = 8000
		self.gain = gain = 20
		self.center_freq = center_freq = 146e6

		##################################################
		# Blocks
		##################################################
		self._gain_layout = Qt.QVBoxLayout()
		self._gain_tool_bar = Qt.QToolBar(self)
		self._gain_layout.addWidget(self._gain_tool_bar)
		self._gain_tool_bar.addWidget(Qt.QLabel("Gain"+": "))
		self._gain_counter = Qwt.QwtCounter()
		self._gain_counter.setRange(0, 50, 1)
		self._gain_counter.setNumButtons(2)
		self._gain_counter.setValue(self.gain)
		self._gain_tool_bar.addWidget(self._gain_counter)
		self._gain_counter.valueChanged.connect(self.set_gain)
		self._gain_slider = Qwt.QwtSlider(None, Qt.Qt.Horizontal, Qwt.QwtSlider.BottomScale, Qwt.QwtSlider.BgSlot)
		self._gain_slider.setRange(0, 50, 1)
		self._gain_slider.setValue(self.gain)
		self._gain_slider.setMinimumWidth(200)
		self._gain_slider.valueChanged.connect(self.set_gain)
		self._gain_layout.addWidget(self._gain_slider)
		self.top_layout.addLayout(self._gain_layout)
		self._center_freq_layout = Qt.QVBoxLayout()
		self._center_freq_tool_bar = Qt.QToolBar(self)
		self._center_freq_layout.addWidget(self._center_freq_tool_bar)
		self._center_freq_tool_bar.addWidget(Qt.QLabel("Center Frequency"+": "))
		self._center_freq_counter = Qwt.QwtCounter()
		self._center_freq_counter.setRange(50e6, 1e9, 1)
		self._center_freq_counter.setNumButtons(2)
		self._center_freq_counter.setValue(self.center_freq)
		self._center_freq_tool_bar.addWidget(self._center_freq_counter)
		self._center_freq_counter.valueChanged.connect(self.set_center_freq)
		self._center_freq_slider = Qwt.QwtSlider(None, Qt.Qt.Horizontal, Qwt.QwtSlider.BottomScale, Qwt.QwtSlider.BgSlot)
		self._center_freq_slider.setRange(50e6, 1e9, 1)
		self._center_freq_slider.setValue(self.center_freq)
		self._center_freq_slider.setMinimumWidth(200)
		self._center_freq_slider.valueChanged.connect(self.set_center_freq)
		self._center_freq_layout.addWidget(self._center_freq_slider)
		self.top_layout.addLayout(self._center_freq_layout)
		self.radioserver_source_c_1 = radioserver_source_c("ws://192.168.220.10/WebSocket/", samp_rate, center_freq, gain)
		self.qtgui_sink_x_0 = qtgui.sink_c(
			1024, #fftsize
			firdes.WIN_BLACKMAN_hARRIS, #wintype
			0, #fc
			samp_rate, #bw
			"QT GUI Plot", #name
			True, #plotfreq
			True, #plotwaterfall
			True, #plottime
			True, #plotconst
		)
		self.qtgui_sink_x_0.set_update_time(1.0 / 10)
		self._qtgui_sink_x_0_win = sip.wrapinstance(self.qtgui_sink_x_0.pyqwidget(), Qt.QWidget)
		self.top_layout.addWidget(self._qtgui_sink_x_0_win)
		self.blocks_throttle_0 = blocks.throttle(gr.sizeof_gr_complex*1, samp_rate)
		self.blocks_multiply_const_vxx_0 = blocks.multiply_const_vcc((1/2**15.+1j*1/2**15., ))

		##################################################
		# Connections
		##################################################
		self.connect((self.blocks_throttle_0, 0), (self.blocks_multiply_const_vxx_0, 0))
		self.connect((self.radioserver_source_c_1, 0), (self.blocks_throttle_0, 0))
		self.connect((self.blocks_multiply_const_vxx_0, 0), (self.qtgui_sink_x_0, 0))


	def get_samp_rate(self):
		return self.samp_rate

	def set_samp_rate(self, samp_rate):
		self.samp_rate = samp_rate
		self.blocks_throttle_0.set_sample_rate(self.samp_rate)
		self.qtgui_sink_x_0.set_frequency_range(0, self.samp_rate)

	def get_gain(self):
		return self.gain

	def set_gain(self, gain):
		self.gain = gain
		self._gain_counter.setValue(self.gain)
		self._gain_slider.setValue(self.gain)

	def get_center_freq(self):
		return self.center_freq

	def set_center_freq(self, center_freq):
		self.center_freq = center_freq
		self._center_freq_counter.setValue(self.center_freq)
		self._center_freq_slider.setValue(self.center_freq)

if __name__ == '__main__':
	parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
	(options, args) = parser.parse_args()
	qapp = Qt.QApplication(sys.argv)
	tb = qa_radioserver_source_top_block()
	tb.start()
	tb.show()
	qapp.exec_()
	tb.stop()

