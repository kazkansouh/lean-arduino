#ifndef _QUEUE_H
#define _QUEUE_H

#include <stdbool.h>
#include <stdint.h>

bool queue_enqueue(uint8_t* const buffer,
                   const uint8_t size,
                   uint8_t* const head,
                   const uint8_t tail,
                   const uint8_t data);

bool queue_dequeue(uint8_t* const buffer,
                   const uint8_t size,
                   const uint8_t head,
                   uint8_t* const tail,
                   uint8_t* const data);

#endif
