# Python SWIG interface for YDB

## Generate SWIG interface for YDB

```shell
swig -python -c++ ydb.i
```

## Install ydb to this directory
```shell
python setup.py build_ext --inplace
```

## Install ydb to python2

```shell
python setup_compile.py build_ext
python setup_compile.py install --user --record ydb-installed.txt
```

## Install YDB to python3

```shell
python3 setup_compile.py build_ext
python3 setup_compile.py install --user --record ydb-installed3.txt
```

## Uninstall YDB from python

```shell
xargs rm -rf < installed.txt
```