#!/bin/bash
set -e

echo "===> Current dir:"
pwd
ls -la

echo "===> Go to /mnt"
cd /mnt

echo "===> Build deb package"
make clean
make deb

echo "===> Install package"
dpkg -i kubsh.deb

echo "===> Check kubsh in PATH"
which kubsh
kubsh --help || true

echo "===> Run pytest"
pytest -v
