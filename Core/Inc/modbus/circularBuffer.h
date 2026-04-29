/**
 * @file circularBuffer.h
 * @brief circular buffer implementation
 * @details Provides a thread-safe circular buffer data structure for efficient
 * FIFO operations with fixed memory footprint, suitable for real-time
 * applications and Modbus communication.
 */
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "main.h"

typedef enum CBError {
	CB_OK = 0,
	CB_ERR_NULL,
	CB_ERR_FULL,
	CB_ERR_EMPTY,
	CB_ERR_SIZE,
	CB_ERR_NOT_INIT,
} CB_Status_t;

typedef struct circular_buf_t {
	uint8_t *buffer;
	uint32_t head;
	uint32_t tail;
	uint32_t max;
} circular_buf_t;

typedef circular_buf_t *cbuf_handle_t;

// Modbus Circular Buffer for UART reception
extern cbuf_handle_t hcbuf_modbus;

cbuf_handle_t cbuf_init(uint8_t *buffer, size_t size);
CB_Status_t cbuf_free(cbuf_handle_t c);
CB_Status_t cbuf_flush(cbuf_handle_t c);
CB_Status_t cbuf_put(cbuf_handle_t c, uint8_t *data, uint32_t length);
CB_Status_t cbuf_get(cbuf_handle_t c, uint8_t *data, uint32_t length);
CB_Status_t cbuf_peek(cbuf_handle_t c, uint8_t *data, uint32_t offset);
uint8_t cbuf_empty(cbuf_handle_t c);
uint8_t cbuf_full(cbuf_handle_t c);
uint32_t cbuf_capacity(cbuf_handle_t c);
uint32_t cbuf_size(cbuf_handle_t c);

#endif