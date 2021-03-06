LIBDIR=../lib

MODULE=\
	main\
	$(LIBDIR)/usart_p\
	$(LIBDIR)/usart\
	$(LIBDIR)/queue\
	$(LIBDIR)/spi\
	$(LIBDIR)/timer\
	$(LIBDIR)/icr-pulse\
	$(LIBDIR)/pwm

PROJECT=main

OBJECTS=$(patsubst %,%.o,$(MODULE))

SERIALBAUD=57600

CC=avr-gcc
CFLAGS=\
	-Os\
	-DF_CPU=16000000UL\
	-DBAUD=$(SERIALBAUD)\
	-DUSE_PRINTF -DPIN_ERROR=9\
	-mmcu=atmega328p\
	-Wall\
	-Wpedantic\
	-Werror\
	-I$(LIBDIR)
LD=avr-gcc
LDFLAGS=-mmcu=atmega328p

default: $(PROJECT).hex

$(PROJECT): $(OBJECTS)

%.hex: %
	avr-objcopy -O ihex -R .eeprom $< $@

flash: $(PROJECT).hex
	avrdude -F -V -c arduino -p ATMEGA328P -P /dev/ttyACM0 -b 115200 -U flash:w:$<

test: flash
	gtkterm --port /dev/ttyACM0 --speed $(SERIALBAUD)

.PHONY: clean default flash test
clean:
	-rm $(PROJECT)
	-rm $(OBJECTS)
	-rm $(PROJECT).hex
