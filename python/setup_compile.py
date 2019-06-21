#!/usr/bin/env python

"""
setup.py file for YDB interface
"""

from distutils.core import setup, Extension

ydb_module = Extension('_ydb',
                       include_dirs=['../ydb', '../ydb/utilities'],
                       libraries=['yaml'],
                       sources=['ydb_wrap.cxx',
                                '../ydb/utilities/art.c',
                                '../ydb/utilities/utf8.c',
                                '../ydb/utilities/base64.c',
                                '../ydb/ylist.c',
                                '../ydb/yarray.c',
                                '../ydb/ytrie.c',
                                '../ydb/ytree.c',
                                '../ydb/ymap.c',
                                '../ydb/ystr.c',
                                '../ydb/ynode.c',
                                '../ydb/ydb.c',
                                '../ydb/yipc.c',
                                '../ydb/ylog.c',
                                '../ydb/ydbcpp.cpp'
                                ],
                       )

# ydb_module = Extension('_ydb',
#                        include_dirs=['../ydb', '../ydb/utilities'],
#                        libraries=['ydb', 'yaml'],
#                        sources=['ydb_wrap.cxx'],
#                        )

setup(name='ydb',
      version='0.1',
      author="neoul@ymail.com",
      description="""Simple YDB interface""",
      ext_modules=[ydb_module],
      py_modules=["ydb"],
      )
