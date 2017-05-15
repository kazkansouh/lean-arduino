#include "queue.h"

bool queue_enqueue(uint8_t* const buffer,
                   const uint8_t size,
                   uint8_t* const head,
                   const uint8_t tail,
                   const uint8_t data) {
  register int newhead;

  if (tail <= *head) {
    newhead = (*head + 1) % size;
  } else /* head < tail */{
    newhead = *head + 1;
  }

  if (newhead != tail) {
    buffer[*head] = data;
    *head = newhead;
    return true;
  } else {
    return false;
  }
}

bool queue_dequeue(uint8_t* const buffer,
                   const uint8_t size,
                   const uint8_t head,
                   uint8_t* const tail,
                   uint8_t* const data) {
  if (head != *tail) {
    *data = buffer[*tail];
    *tail = (*tail + 1) % size;
    return true;
  }
  return false;
}
