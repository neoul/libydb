#!/bin/bash
source ~/.bashrc
#echo $BASH_SOURCE
SOURCEPATH=$(dirname "$(readlink -f $BASH_SOURCE)")
echo $SOURCEPATH
# export GO111MODULE=on
export GOPATH=$SOURCEPATH
export GOBIN=$HOME/go/bin
PATH1=${PATH%:${GOBIN}}
export PATH=${PATH1}:${GOBIN}

export GRPC_VERBOSITY=DEBUG
export GRPC_TRACE=tcp,secure_endpoint,transport_security
export GRPC_GO_LOG_VERBOSITY_LEVEL=99
export GRPC_GO_LOG_SEVERITY_LEVEL=info
