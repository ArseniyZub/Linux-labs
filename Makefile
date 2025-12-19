CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE -std=c11
LDFLAGS = -lreadline -lfuse3

TARGET = kubsh

VERSION = 1.0.0
PACKAGE_NAME = kubsh
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64
DEB_DEPENDS = libreadline8, fuse3, passwd

.PHONY: all clean run install uninstall prepare-deb deb test

# -------------------------
# Сборка
# -------------------------
all: $(TARGET)

$(TARGET): vfs.o kubsh.o
	$(CC) $(CFLAGS) vfs.o kubsh.o -o $(TARGET) $(LDFLAGS)

vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -c vfs.c -o vfs.o

kubsh.o: kubsh.c vfs.h
	$(CC) $(CFLAGS) -c kubsh.c -o kubsh.o

# -------------------------
# Запуск
# -------------------------
run: $(TARGET)
	./$(TARGET)

# -------------------------
# Установка (setuid-root)
# -------------------------
install: $(TARGET)
	@echo "Установка kubsh с setuid-root"
	sudo install -m 4755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# -------------------------
# Очистка
# -------------------------
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.deb *.o

# -------------------------
# Подготовка deb
# -------------------------
prepare-deb: $(TARGET)
	@echo "Подготовка deb-пакета..."
	@rm -rf $(BUILD_DIR)
	@mkdir -p $(DEB_DIR)/DEBIAN
	@mkdir -p $(DEB_DIR)/usr/local/bin
	@mkdir -p $(DEB_DIR)/usr/share/doc/$(PACKAGE_NAME)

	@cp $(TARGET) $(DEB_DIR)/usr/local/bin/
	@chmod 755 $(DEB_DIR)/usr/local/bin/$(TARGET)

	@echo "Package: $(PACKAGE_NAME)" > $(DEB_DIR)/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Section: utils" >> $(DEB_DIR)/DEBIAN/control
	@echo "Priority: optional" >> $(DEB_DIR)/DEBIAN/control
	@echo "Architecture: amd64" >> $(DEB_DIR)/DEBIAN/control
	@echo "Maintainer: $(USER) <$(USER)@localhost>" >> $(DEB_DIR)/DEBIAN/control
	@echo "Depends: $(DEB_DEPENDS)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Description: Custom shell" >> $(DEB_DIR)/DEBIAN/control
	@echo " KubSH is a custom shell with FUSE-based VFS" >> $(DEB_DIR)/DEBIAN/control

	@echo "MIT License" > $(DEB_DIR)/usr/share/doc/$(PACKAGE_NAME)/copyright

	@echo "#!/bin/sh" > $(DEB_DIR)/DEBIAN/postinst
	@echo "chmod u+s /usr/local/bin/$(TARGET)" >> $(DEB_DIR)/DEBIAN/postinst
	@chmod 755 $(DEB_DIR)/DEBIAN/postinst

# -------------------------
# Сборка deb
# -------------------------
deb: prepare-deb
	@echo "Сборка deb-пакета..."
	dpkg-deb --build --root-owner-group $(DEB_DIR)
	@cp $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64.deb kubsh.deb
	@echo "Создан kubsh.deb"

# -------------------------
# Тесты (Docker)
# -------------------------
test: deb
	@echo "Запуск тестов в Docker..."
	sudo docker run -v $(PWD):/mnt tyvik/kubsh_test:master
