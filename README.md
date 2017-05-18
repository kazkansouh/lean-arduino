# lean-arduino

The beginnings of a simple c library for Arduino / ``atmega328p`` that
does no rely on the Arduino/Wiring libraries. It is directly built
using the AVR tool chain and the ``c`` library.

I've started this project as while working with Arduino standard library it
became hard to understand why some of the timings involving interrupts
were not correct, so this project is to give me more understanding and
control over the timings.

# features

Currently supported:
* SPI in master mode (blocking, i.e. w/o interrupts)
* USART (with interrupts)
* millisecond timer (with interrupts)
* microsecond pulse width detection (with interrupts using ICP1/ICR1)
* pwm with prescaler of 32 to drive an led

# sample application

The main program utilises these features to respond to a HC-SR04 by
changing the number of illuminated leds driven by a 74HC595 that uses
SPI to shift out the data.