#!/bin/bash

docker build --rm --target frs -t lanta/frs -f Dockerfile ..
docker image prune --force --filter label=stage=builder
