#!/bin/bash
# Copyright (c) 2019-2025 The Khronos Group Inc.
# Copyright (c) 2019 Collabora, Ltd.
# SPDX-License-Identifier: Apache-2.0

# Start the docker image named below, for spec generation, etc.
# Mounts this directory as the mountpoint mentioned below,
# and logs you in as a normal user whose user ID and group ID match
# your outside-of-container user, thus avoiding mangling permissions.
# Container is automatically removed when you exit it.

# The docker images used are published here:
# https://hub.docker.com/r/khronosgroup/docker-images/tags
#
# with their dockerfile located at:
# https://github.com/KhronosGroup/DockerContainers

# This image/tag is generated from https://github.com/KhronosGroup/DockerContainers/blob/main/openxr.Dockerfile
# Purpose: Spec (pdf/html) generation
IMAGE_NAME=khronosgroup/docker-images:openxr.20240805
MOUNTPOINT=$(pwd)

set -e
# docker pull $IMAGE_NAME
uid=$(id -u)
gid=$(id -g)
docker run -it --rm \
    --entrypoint /root/entrypoint.openxr.sh \
    -e "USER_ID=$uid" \
    -e "GROUP_ID=$gid" \
    -e "CONTAINER_CWD=$MOUNTPOINT" \
    -v "$MOUNTPOINT:$MOUNTPOINT" \
    $IMAGE_NAME "$@"
