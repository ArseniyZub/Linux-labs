CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE
LDFLAGS = -lreadline -lfuse3 -lpthread

SRC = kubsh.c vfs.c
TARGET = kubsh

VERSION = 1.0.0
PACKAGE_NAME = kubsh
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64

DEB_DEPENDS = libreadline8, fuse3

all: build

build:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run: build
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)

prepare-deb: build
	mkdir -p $(DEB_DIR)/DEBIAN
	mkdir -p $(DEB_DIR)/usr/local/bin
	mkdir -p $(DEB_DIR)/usr/share/doc/$(PACKAGE_NAME)

	cp $(TARGET) $(DEB_DIR)/usr/local/bin/
	chmod 755 $(DEB_DIR)/usr/local/bin/$(TARGET)

	echo "Package: $(PACKAGE_NAME)"              >  $(DEB_DIR)/DEBIAN/control
	echo "Version: $(VERSION)"                  >> $(DEB_DIR)/DEBIAN/control
	echo "Section: utils"                       >> $(DEB_DIR)/DEBIAN/control
	echo "Priority: optional"                   >> $(DEB_DIR)/DEBIAN/control
	echo "Architecture: amd64"                  >> $(DEB_DIR)/DEBIAN/control
	echo "Maintainer: $(USER)"                  >> $(DEB_DIR)/DEBIAN/control
	echo "Description: Custom shell"            >> $(DEB_DIR)/DEBIAN/control
	echo "Depends: $(DEB_DEPENDS)"              >> $(DEB_DIR)/DEBIAN/control

deb: prepare-deb
	dpkg-deb --build --root-owner-group $(DEB_DIR)
	cp $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64.deb kubsh.deb
	@echo "Готово: kubsh.deb"

test: deb 
	sudo docker run -v $(PWD):/mnt tyvik/kubsh_test:master

.PHONY: all build run clean prepare-deb deb