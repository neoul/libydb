# Python SWIG interface for YDB

## Generate SWIG interface for YDB

```shell
swig -python -c++ ydb.i
```

## Install ydb to this directory
```shell
python setup.py build_ext --inplace --record installed.txt
```

## Install ydb to python2

```shell
python setup_compile.py build_ext --record installed.txt
python setup_compile.py install --user
```

## Install YDB to python3

```shell
python3 setup_compile.py build_ext --record installed.txt
python3 setup_compile.py install --user
```

## Uninstall YDB from python

```shell
xargs rm -rf < installed.txt
```