
CFLAGS=-fPIC -DHAVE_CONFIG_H -I /usr/include/libusb-1.0 
LDFLAGS=-shared -lusb-1.0 
CC=gcc

HYGMOD=hygusb.so
DESTDIR?=

.SUFFIXES: .o .c .so

all: $(HYGMOD)

clean:
	$(RM) $(HYGMOD)  $(HYGMOD:.so=.o)
	@echo cleanup done

install: $(HYGMOD)
	@install $< $(DESTDIR)/usr/lib/collectd/
.o.so: 
	$(LD) $(LDFLAGS) -o $@ $<
