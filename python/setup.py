#!/usr/bin/env python

"""
setup.py file for YDB interface
"""

from distutils.core import setup, Extension

ydb_module = Extension('_ydb',
                       include_dirs=['../ydb', '../ydb/utilities'],
                       libraries=['ydb', 'yaml'],
                       sources=['ydb_wrap.cxx', 'ydbcpp.cpp'],
                       )

setup(name='ydb',
      version='0.1',
      author="neoul@ymail.com",
      description="""Simple YDB interface""",
      ext_modules=[ydb_module],
      py_modules=["ydb"],
      )
