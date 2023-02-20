#!/bin/bash

GIT_VERSION=$(git describe --tags | sed 's/v//g')
echo $GIT_VERSION