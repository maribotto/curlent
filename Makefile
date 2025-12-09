CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 $(shell pkg-config --cflags libtorrent-rasterbar)
LDFLAGS = $(shell pkg-config --libs libtorrent-rasterbar) -lpthread

TARGET = curlent
SRC = curlent.cpp

PREFIX = /usr/local

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	@if [ -z "$(DESTDIR)" ] && [ ! -f $(HOME)/.config/curlent/config ]; then \
		mkdir -p $(HOME)/.config/curlent; \
		cp config.example $(HOME)/.config/curlent/config; \
		echo "Installed config to ~/.config/curlent/config"; \
	fi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

.PHONY: all clean install uninstall
