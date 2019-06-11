#! /bin/bash

swig -python -c++ ydb.i
python setup.py build_ext --inplace