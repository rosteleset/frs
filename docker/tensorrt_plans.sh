#!/bin/bash

# Используемые переменные
# FRS_HOST_WORKDIR - рабочая директория FRS на сервере
# TRITON_VERSION

docker pull nvcr.io/nvidia/tritonserver:$TRITON_VERSION-py3

# scrfd
mkdir -p $FRS_HOST_WORKDIR/model_repository/scrfd/1/
docker run --gpus all --rm -v $FRS_HOST_WORKDIR:/opt/frs --entrypoint=bash nvcr.io/nvidia/tritonserver:$TRITON_VERSION-py3 -c \
	"/usr/src/tensorrt/bin/trtexec --onnx=/opt/frs/model_repository_onnx/scrfd/1/model.onnx --saveEngine=/opt/frs/model_repository/scrfd/1/model.plan --shapes=input.1:1x3x320x320"

# genet
mkdir -p $FRS_HOST_WORKDIR/model_repository/genet/1/
docker run --gpus all --rm -v $FRS_HOST_WORKDIR:/opt/frs --entrypoint=bash nvcr.io/nvidia/tritonserver:$TRITON_VERSION-py3 -c \
	"/usr/src/tensorrt/bin/trtexec --onnx=/opt/frs/model_repository_onnx/genet/1/model.onnx --saveEngine=/opt/frs/model_repository/genet/1/model.plan"

# arcface
mkdir -p $FRS_HOST_WORKDIR/model_repository/arcface/1/
docker run --gpus all --rm -v $FRS_HOST_WORKDIR:/opt/frs --entrypoint=bash nvcr.io/nvidia/tritonserver:$TRITON_VERSION-py3 -c \
	"/usr/src/tensorrt/bin/trtexec --onnx=/opt/frs/model_repository_onnx/arcface/1/model.onnx --saveEngine=/opt/frs/model_repository/arcface/1/model.plan --shapes=input.1:1x3x112x112"
