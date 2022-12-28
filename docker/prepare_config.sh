#!/bin/bash

# Используемые переменные
# FRS_HOST_WORKDIR - рабочая директория FRS на сервере
# MYSQL_DB
# MYSQL_PORT
# MYSQLX_PORT
# MYSQL_PASSWORD
# TRITON_VERSION
# WITH_GPU

TRITON_VERSION="${TRITON_VERSION:=22.12}"
export TRITON_VERSION

mkdir -p $FRS_HOST_WORKDIR/mysql/

cp $(pwd)/../mysql/01_structure.sql $FRS_HOST_WORKDIR/mysql/01_structure.sql
sed 's/localhost:/triton:/g' $(pwd)/../mysql/02_default_data.sql > $FRS_HOST_WORKDIR/mysql/02_default_data.sql

envsubst \$MYSQL_DB,\$MYSQLX_PORT,\$MYSQL_PASSWORD < frs.config > $FRS_HOST_WORKDIR/frs.config
envsubst \$FRS_HOST_WORKDIR,\$MYSQL_DB,\$MYSQL_PORT,\$MYSQLX_PORT,\$MYSQL_PASSWORD,\$TRITON_VERSION < docker-compose-cpu.yml > $FRS_HOST_WORKDIR/docker-compose-cpu.yml
envsubst \$FRS_HOST_WORKDIR,\$MYSQL_DB,\$MYSQL_PORT,\$MYSQLX_PORT,\$MYSQL_PASSWORD,\$TRITON_VERSION < docker-compose-gpu.yml > $FRS_HOST_WORKDIR/docker-compose-gpu.yml

FRS_OPTDIR=$(pwd)/../opt
cp -r $FRS_OPTDIR/model_repository_onnx $FRS_HOST_WORKDIR
mkdir -p $FRS_HOST_WORKDIR/model_repository_onnx/genet/1/
cp $FRS_OPTDIR/plan_source/genet/genet_small_custom_ft.onnx $FRS_HOST_WORKDIR/model_repository_onnx/genet/1/model.onnx
mkdir -p $FRS_HOST_WORKDIR/model_repository_onnx/scrfd/1/
cp $FRS_OPTDIR/plan_source/scrfd/scrfd_10g_bnkps.onnx $FRS_HOST_WORKDIR/model_repository_onnx/scrfd/1/model.onnx
wget --load-cookies /tmp/cookies.txt "https://docs.google.com/uc?export=download&confirm= \
	$(wget --quiet --save-cookies /tmp/cookies.txt --keep-session-cookies --no-check-certificate 'https://docs.google.com/uc?export=download&id=1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP' \
	-O- | sed -rn 's/.*confirm=([0-9A-Za-z_]+).*/\1\n/p')&id=1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP" -O $FRS_HOST_WORKDIR/model_repository_onnx/arcface/1/model.onnx && rm -rf /tmp/cookies.txt

if [[ $WITH_GPU -eq 1 ]]; then
  export FRS_HOST_WORKDIR
  cp -r $FRS_OPTDIR/model_repository $FRS_HOST_WORKDIR
  ./tensorrt_plans.sh
fi
