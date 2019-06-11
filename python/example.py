#!/usr/bin/env python
# neoul@ymail.com

"""
YDB SWIG interface for Python
"""
import os
import yaml
import subprocess
import time
import ydb

if __name__ == "__main__":

    subprocess.check_call("ydb -r pub -a uss://hello -d -c -v debug &", shell=True)
    time.sleep(1)

    y = ydb.Ydb("example")
    y.connect("uss://hello", "pub")
    top = os.path.dirname(os.path.abspath(__file__))
    with open(top + '/../examples/yaml/ydb-input.yaml', "r") as f:
        data = f.read()
        y.push(data)
    # print(y.get())
    y.serve(0)
    time.sleep(1)
    y.serve(0)
    time.sleep(1)
    subprocess.check_call("killall ydb", shell=True)
    