#!/usr/bin/env python
#
# Copyright 2012 Peter Desnoyers
# This file is part of ftlsim.
# ftlsim is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version. 
#
# ftlsim is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details. 
# You should have received a copy of the GNU General Public License
# along with ftlsim. If not, see http://www.gnu.org/licenses/. 
#
"""
setup.py file for ftlsim
"""

from distutils.core import setup, Extension


sim_module = Extension('_ftlsim', sources=['ftlsim.i', 'ftlsim.c',])
adr_module = Extension('_getaddr', sources=['getaddr.i', 'getaddr.c'] )
w_module = Extension('_lambertw', sources=['lambertw.i', 'lambertw.c'] )

setup (name = 'newsim',
       version = '0.2',
       author      = "Peter Desnoyers",
       author_email = "pjd@ccs.neu.edu",
       description = """Modular high-speed FTL simulator""",
       ext_modules = [sim_module, adr_module, w_module],
       py_modules = ["genaddr", "ftlsim", "getaddr", "lambertw"]
       )
