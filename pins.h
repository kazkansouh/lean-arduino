#ifndef _PINS_H
#define _PINS_H

#define _CAT(x,y) x ## y
#define _CAT2(x,y,z) x ## y(z)

#define _pinToPort_0 D
#define _pinToPort_1 D
#define _pinToPort_2 D
#define _pinToPort_3 D
#define _pinToPort_4 D
#define _pinToPort_5 D
#define _pinToPort_6 D
#define _pinToPort_7 D

#define _pinToPort_8  B
#define _pinToPort_9  B
#define _pinToPort_10 B
#define _pinToPort_11 B
#define _pinToPort_12 B
#define _pinToPort_13 B
#define _pinToPort_14 B
#define _pinToPort_15 B

#define _pinToPort(pin) _pinToPort_ ## pin

#define _SUBTRACT_8_8 0
#define _SUBTRACT_9_8 1
#define _SUBTRACT_10_8 2
#define _SUBTRACT_11_8 3
#define _SUBTRACT_12_8 4
#define _SUBTRACT_13_8 5
#define _SUBTRACT_14_8 6
#define _SUBTRACT_15_8 7

#define _SUBTRACT(n,m) _SUBTRACT_ ## n ## _ ## m

#define _pinPortToPin_B_AUX(pin) _CAT(B, pin)
#define _pinPortToPin_B(pin) _pinPortToPin_B_AUX(_SUBTRACT(pin, 8))
#define _pinPortToPin_D(pin) _CAT(D, pin)

#define _pinPortToPin(pin,port) _CAT2(_pinPortToPin_,port,pin)

#define _pinToPin_AUX(name,pin) _CAT(name,pin)
#define _pinToPin(name,pin) \
  _pinToPin_AUX(name,_pinPortToPin(pin,_pinToPort(pin)))

#define _operation_true |=
#define _operation_false &= ~
#define _operation_1 |=
#define _operation_0 &= ~

#define _operation(value) _operation_ ## value

#define _write_pin_AUX(port,pin,value)                          \
  _CAT(PORT,port) _operation(value) _BV(_pinToPin(PORT,pin))
#define writePin(pin,value) _write_pin_AUX(_pinToPort(pin), pin, value)

#define _read_pin_AUX(port, pin) (_CAT(PIN, port) & _BV(_pinToPin(PORT,pin)))
#define readPin(pin) _read_pin_AUX(_pinToPort(pin), pin)

#define _COMP(a,b) a(b)

#define _set_pin_mode_AUX(port,pin,value)                       \
  _CAT(DDR,port) _operation(value) _BV(pin)
#define _set_pin_output(pin)                                    \
  _set_pin_mode_AUX(_pinToPort(pin), _pinToPin(DD,pin), true)
#define _set_pin_input(pin)                                     \
  _set_pin_mode_AUX(_pinToPort(pin), _pinToPin(DD,pin), false); \
  writePin(pin,false)
#define _set_pin_input_pullup(pin)                              \
  _set_pin_mode_AUX(_pinToPort(pin), _pinToPin(DD,pin), true);  \
  writePin(pin,true)

#define setMode(pin,mode) _set_pin_ ## mode(pin)

#endif
