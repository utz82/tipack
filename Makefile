prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

CC = gcc
CFLAGS = -g -O -W -Wall

cflag = $(DEFS) $(CFLAGS) $(CPPFLAGS) `pkg-config --cflags tifiles2 ticonv glib-2.0`
libs = $(LDFLAGS) $(LIBS) `pkg-config --libs glib-2.0 tifiles2 ticonv`

tipack: tipack.c
	$(CC) $(cflag) tipack.c $(libs) -o tipack

clean:
	rm -f tipack

install: tipack
	cp -p tipack $(DESTDIR)$(bindir)/
	chmod 755 $(DESTDIR)$(bindir)/tipack

.PHONY: install clean
