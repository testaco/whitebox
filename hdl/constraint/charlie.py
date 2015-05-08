"""Generate Libero constraint files, based on schematic and datasheet information.

This tool automates one of the more mind-bending aspects of putting together
an FPGA SoC project - mapping up pins from the thing you're working on, to the
underlying FPGA's pin ids.  A constraint file defines the mapping from your
pins to the FPGA's naming convention.  It becomes part of the firmware blob
that is flashed onto the chip on startup.

In the Whitebox Charlie case, we're mapping this topology:

         Whitebox <-------> M2S

Whitebox describes pins for the card, accessing the ADC, DAC, RADIO and VCO.
BSB-EXT describes pins for the baseboard, wich the Whitebox mates to.
SOM describes pins in Emcraft System on Module lingo, GPIO_*.
A2F describes actual pin numbers on the Actel SmartFusion chip.

This script will hop the two BSB-EXT and SOM domains to map from Whitebox
to A2F.  This is then combined in a constraint file, which is a simple tcl
script that Libero will use.
"""

import os
import re
from Cheetah.Template import Template

class Pin:
    def __init__(self, pin_des):
        try:
            self._points_to = []
            self.parse(pin_des)
        except Exception, e:
            raise Exception, 'Error parsing pin description: "%s"\nWhat: %s' \
                % (pin_des, e)

    def parse(self, pin_des):
        x = re.match('^([^\s]+)\s*([^\s]+)\s*([^\s]*)$', pin_des)
        
        k, v, self.direction = x.groups()

        self.name = k
        self.points_to(v)

    def points_to(self, to):
        self._points_to.append(to)

    def current_pointer(self):
        return self._points_to[-1]

def parse_mapping(config_filename):
    config_f = None
    try:
        config_f = open(config_filename, 'r')
        config = config_f.read()
        mapping = {}
        for pin_des in config.splitlines():
            if pin_des.strip() and not pin_des.startswith('#'):
                p = Pin(pin_des)
                mapping[p.name] = p
        return mapping
    finally:
        if config_f:
            config_f.close()

def do_mapping():
    pin_mapping = parse_mapping(os.path.join(options.src_dir, 'whitebox-charlie.pinmap'))
    m2s_mapping = parse_mapping(os.path.join(options.src_dir, 'm2s.pinmap'))

    pin_names = pin_mapping.iterkeys()

    for p_name in sorted(pin_names):
        p = pin_mapping[p_name]
        p.points_to(m2s_mapping[p.current_pointer()].current_pointer())

    return pin_mapping

def parse_tmpl(_tmpl_text, **kwargs):
    return str(Template(_tmpl_text, kwargs))

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option('-s', '--src-dir', dest='src_dir',
        help="Source directory")
    parser.add_option('-b', '--binary-dir', dest='binary_dir',
        help="Binary directory")
    options, args = parser.parse_args()

    pin_mapping = do_mapping()
    pin_names = sorted(pin_mapping.iterkeys())

    designer_pdc_tmpl_f = None
    designer_pdc_f = None
    try:
        designer_pdc_tmpl_f = open(os.path.join(options.src_dir, 'designer.pdc.tmpl'), 'r')
        designer_pdc = parse_tmpl(designer_pdc_tmpl_f.read(),
            pin_names=pin_names,
            pin_mapping=pin_mapping)
        designer_pdc_f = open(os.path.join(options.binary_dir, 'designer.pdc'), 'w')
        designer_pdc_f.write(designer_pdc)
    finally:
        if designer_pdc_tmpl_f: designer_pdc_tmpl_f.close()
        if designer_pdc_f: designer_pdc_f.close()
