#!/usr/bin/env python
# Copyright 2013 Chris Testa <testac@gmail.com>
#
# Based off of work
# Copyright 2010-2011 Ettus Research LLC
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os.path
import re
import sys
import math
from Cheetah.Template import Template

HEADER_TMPL = """
#import time
/***********************************************************************
 * This file was generated by $file on $time.strftime("%c")
 **********************************************************************/
\#ifndef INCLUDED_$(name.upper())_H
\#define INCLUDED_$(name.upper())_H

\#include <stdint.h>
\#include <stdio.h>

#for $reg in $regs
  #if $reg.get_enums()

typedef enum {
    #for $i, $enum in enumerate($reg.get_enums())
    #set $end_comma = ',' if $i < len($reg.get_enums())-1 else ''
    $(reg.get_name().upper())_$(enum[0].upper()) = $enum[1]$end_comma
    #end for
} $reg.get_type();
  #end if
#end for

typedef struct {
    #for $reg in $regs
    $reg.get_type() $reg.get_name();
    #end for
} $(name)_t;

void $(name)_init($(name)_t* rf);
void $(name)_destroy($(name)_t* rf);
void $(name)_copy($(name)_t* src, $(name)_t* dst);
void $(name)_print_to_file($(name)_t* rf, FILE* f);

    #for $mreg in $mregs
$mreg.get_type() $(name)_get_$(mreg.get_name())($(name)_t* rf);
void $(name)_set_$(mreg.get_name())($(name)_t* rf, $mreg.get_type() reg);
    #end for

$header_impl

\#endif /* INCLUDED_$(name.upper())_H */
"""

SRC_TMPL = """
\#include "$(name).h"
/***********************************************************************
 * This file was generated by $file on $time.strftime("%c")
 **********************************************************************/
void $(name)_init($(name)_t* rf) {
    #for $reg in $regs
    rf->$reg.get_name() = $reg.get_default();
    #end for
}

void $(name)_copy($(name)_t* src, $(name)_t* dst) {
    #for $reg in $regs
    dst->$reg.get_name() = src->$reg.get_name();
    #end for
}

void $(name)_destroy($(name)_t* rf) {
}

void $(name)_print_to_file($(name)_t* rf, FILE* f) {
    #for $reg in $regs
        #if $reg.get_enums()
        #for $i, $enum in enumerate($reg.get_enums())
    if (rf->$reg.get_name() == $(reg.get_name().upper())_$(enum[0].upper())) fprintf(f, "$(reg.get_name())=$(enum[0].upper())\\n");
        #end for
        #else
    fprintf(f, "$reg.get_name()=%d\\n", rf->$reg.get_name());
        #end if
    #end for

    #for $mreg in $mregs
    fprintf(f, "$mreg.get_name()=%d\\n", $(name)_get_$(mreg.get_name())(rf));
    #end for
}

    #for $mreg in $mregs
$mreg.get_type() $(name)_get_$(mreg.get_name())($(name)_t* rf) {
    return
    #set $shift = 0
    #for $reg in $mreg.get_regs()
    (($(mreg.get_type()))((rf->$reg.get_name() & $reg.get_mask()) << $shift)) |
       #set $shift = $shift + $reg.get_bit_width()
    #end for
    0;
}
void $(name)_set_$(mreg.get_name())($(name)_t* rf, $mreg.get_type() reg) {
    #set $shift = 0
    #for $reg in $mreg.get_regs()
    rf->$reg.get_name() = (reg >> $shift) & $reg.get_mask();
        #set $shift = $shift + $reg.get_bit_width()
    #end for
}
    #end for

$src_impl
"""
#    $body
#
#    template<typename T> std::set<T> get_changed_addrs(void){
#        if (_state == NULL) throw uhd::runtime_error("no saved state");
#        //check each register for changes
#        std::set<T> addrs;
#        #for $reg in $regs
#        if(_state->$reg.get_name() != this->$reg.get_name()){
#            addrs.insert($reg.get_addr());
#        }
#        #end for
#        return addrs;
#    }
#
#    #end for
#private:
#    $(name)_t *_state;
#};
#
#"""

def parse_tmpl(_tmpl_text, **kwargs):
    return str(Template(_tmpl_text, kwargs))

def to_num(arg): return int(eval(arg))

class reg:
    def __init__(self, reg_des):
        try: self.parse(reg_des)
        except Exception, e:
            raise Exception, 'Error parsing register description: "%s"\nWhat: %s'%(reg_des, e)

    def parse(self, reg_des):
        x = re.match('^(\w*)\s*(\w*)\[(.*)\]\s*(\w*)\s*(.*)$', reg_des)
        name, addr, bit_range, default, enums = x.groups()

        #store variables
        self._name = name
        self._addr = to_num(addr)

        if ':' in bit_range: self._addr_spec = sorted(map(int, bit_range.split(':')))
        else: self._addr_spec = int(bit_range), int(bit_range)
        self._default = to_num(default)

        #extract enum
        self._enums = list()
        if enums:
            enum_val = 0
            for enum_str in map(str.strip, enums.split(',')):
                if '=' in enum_str:
                    enum_name, enum_val = enum_str.split('=')
                    enum_val = to_num(enum_val)
                else: enum_name = enum_str
                self._enums.append((enum_name, enum_val))
                enum_val += 1

    def get_addr(self): return self._addr
    def get_enums(self): return self._enums
    def get_name(self): return self._name
    def get_default(self):
        for key, val in self.get_enums():
            if val == self._default: return str.upper('%s_%s'%(self.get_name(), key))
        return self._default
    def get_type(self):
        if self.get_enums(): return '%s_t'%self.get_name()
        return 'uint%d_t'%max(2**math.ceil(math.log(self.get_bit_width(), 2)), 8)
    def get_shift(self): return self._addr_spec[0]
    def get_mask(self): return hex(int('1'*self.get_bit_width(), 2))
    def get_bit_width(self): return self._addr_spec[1] - self._addr_spec[0] + 1

class mreg:
    def __init__(self, mreg_des, regs):
        try: self.parse(mreg_des, regs)
        except Exception, e:
            raise Exception, 'Error parsing meta register description: "%s"\nWhat: %s'%(mreg_des, e)

    def parse(self, mreg_des, regs):
        x = re.match('^~(\w*)\s+(.*)\s*$', mreg_des)
        self._name, reg_names = x.groups()
        regs_dict = dict([(reg.get_name(), reg) for reg in regs])
        self._regs = [regs_dict[reg_name] for reg_name in map(str.strip, reg_names.split(','))]

    def get_name(self): return self._name
    def get_regs(self): return self._regs
    def get_bit_width(self): return sum(map(reg.get_bit_width, self._regs))
    def get_type(self):
        return 'uint%d_t'%max(2**math.ceil(math.log(self.get_bit_width(), 2)), 8)

def generate(name, regs_tmpl, build_dir, src_dir, body_tmpl='', file=__file__, append=False):
    #evaluate the regs template and parse each line into a register
    regs = list(); mregs = list()
    for entry in parse_tmpl(regs_tmpl).splitlines():
        if entry.startswith('~'): mregs.append(mreg(entry, regs))
        else:                     regs.append(reg(entry))

    #evaluate the code template with the parsed registers and arguments
    header_impl = ''
    if os.path.isfile(os.path.join(src_dir, name + '_impl.h.tmpl')):
        f = open(os.path.join(src_dir, name + '_impl.h.tmpl'));
        header_impl = parse_tmpl(f.read(), name=name, regs=regs, mregs=mregs)
        f.close()
    header = parse_tmpl(HEADER_TMPL,
        name=name,
        regs=regs,
        mregs=mregs,
        header_impl=header_impl,
        file=file,
    )

    #write the generated code to file specified by argv1
    f = open(os.path.join(build_dir, name + '.h'), 'a' if append else 'w')
    f.write(header)
    f.close()

    #evaluate the body template with the list of registers
    src_impl = ''
    if os.path.isfile(os.path.join(src_dir, name + '_impl.c.tmpl')):
        f = open(os.path.join(src_dir, name + '_impl.c.tmpl'));
        src_impl = parse_tmpl(f.read(), name=name, regs=regs, mregs=mregs)
        f.close()
    src = parse_tmpl(SRC_TMPL, name=name, regs=regs, mregs=mregs, src_impl=src_impl, file=file)
    f = open(os.path.join(build_dir, name + '.c'), 'a' if append else 'w')
    f.write(src)
    f.close()

def parseregs(regsstring):
    #evaluate the regs template and parse each line into a register
    regs = list(); mregs = list()
    for entry in parse_tmpl(regsstring).splitlines():
        if entry.startswith('~'): mregs.append(mreg(entry, regs))
        else:                     regs.append(reg(entry))
    return regs, mregs

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option('-n', '--name',
        help="Name of the IC")
    parser.add_option('-s', '--src-dir', dest='src_dir',
        help="Source directory")
    parser.add_option('-b', '--build-dir', dest='build_dir',
        help="Build directory")
    options, args = parser.parse_args()
    print 'parsed'

    regs_tmpl_f = None
    try:
        regs_tmpl_f = open(os.path.join(options.src_dir, options.name + '.regs'), 'r')
        generate(name=options.name,
            regs_tmpl=regs_tmpl_f.read(),
            src_dir=options.src_dir,
            build_dir=options.build_dir
            )

    finally:
        if regs_tmpl_f: regs_tmpl_f.close()
