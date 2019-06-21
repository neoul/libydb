# Python SWIG interface for YDB

```shell
swig -python -c++ ydb.i
python setup.py build_ext --inplace
```

## Install ydb
```shell
python setup_compile.py build_ext
python setup_compile.py install --user
```

## Install YDB to python3

```shell
python3 setup_compile.py build_ext
python3 setup_compile.py install --user
```
