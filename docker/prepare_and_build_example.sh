#!/bin/bash

FRS_HOST_WORKDIR=/opt/frs MYSQL_DB=db_frs MYSQL_PORT=3306 MYSQLX_PORT=33060 MYSQL_PASSWORD=123123 WITH_GPU=1 ./prepare_config.sh
./build_frs.sh
