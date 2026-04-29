#include "modbus/circularBuffer.h"
#include "Utilities/log.h"
#include <string.h>

cbuf_handle_t hcbuf_modbus;

cbuf_handle_t cbuf_init(uint8_t *buffer, size_t size) {
	if (buffer == NULL || size == 0) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return NULL;
	}
	cbuf_handle_t c = malloc(sizeof(circular_buf_t));
	if (c == NULL) {
		LOG_ERROR("Failed to allocate memory for circular buffer!");
		return NULL;
	}
	c->buffer = buffer;
	c->max = size;
	c->head = 0;
	c->tail = 0;

	return c;
}

CB_Status_t cbuf_free(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return CB_ERR_NULL;
	}
	free(c);
	if (c != NULL) {
		LOG_ERROR("Circular buffer was not freed!");
		return CB_ERR_NULL;
	}
	return CB_OK;
}

CB_Status_t cbuf_flush(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return CB_ERR_NULL;
	}
	c->head = 0;
	c->tail = 0;
	return CB_OK;
}

CB_Status_t cbuf_put(cbuf_handle_t c, uint8_t *data, uint32_t length) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return CB_ERR_NULL;
	}
	if (cbuf_full(c)) {
		LOG_ERROR("Circular buffer full!");
		return CB_ERR_FULL;
	}
	if (c->max - cbuf_size(c) < length) {
		LOG_ERROR("Not enough space in buffer");
		return CB_ERR_SIZE;
	}

	if (c->head + length > c->max) {
		memcpy(&c->buffer[c->head], data, c->max - c->head);
		memcpy(&c->buffer[0], &data[c->max - c->head], length - (c->max - c->head));
		c->head = length - (c->max - c->head);
	} else {
		memcpy(&c->buffer[c->head], data, length);
		c->head += length;
	}

	if (c->head == c->max) {
		c->head = 0;
	}
	return CB_OK;
}

CB_Status_t cbuf_get(cbuf_handle_t c, uint8_t *data, uint32_t length) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return CB_ERR_NULL;
	}
	if (cbuf_empty(c)) {
		LOG_ERROR("Circular buffer empty!");
		return CB_ERR_EMPTY;
	}
	if (cbuf_size(c) < length) {
		LOG_ERROR("Not enough data in buffer!");
		return CB_ERR_SIZE;
	}

	if (c->tail + length > c->max) {
		memcpy(data, &c->buffer[c->tail], c->max - c->tail);
		memcpy(data, &c->buffer[0], length - (c->max - c->tail));
		c->tail = length - (c->max - c->tail);
	} else {
		memcpy(data, &c->buffer[c->tail], length);
		c->tail += length;
	}

	if (c->tail == c->max) {
		c->tail = 0;
	}
	return CB_OK;
}

CB_Status_t cbuf_peek(cbuf_handle_t c, uint8_t *data, uint32_t offset) {
	uint32_t index;

	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return CB_ERR_NULL;
	}
	if (cbuf_empty(c)) {
		LOG_ERROR("Circular buffer empty!");
		return CB_ERR_EMPTY;
	}
	if (cbuf_size(c) < offset) {
		LOG_ERROR("Not enough data in buffer!");
		return CB_ERR_SIZE;
	}

	index = c->tail + offset;
	if (index > c->max) {
		index -= c->max;
	}

	memcpy(data, &c->buffer[index], sizeof(uint8_t));
	return CB_OK;
}

uint8_t cbuf_empty(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return 2;
	}
	return (c->head == c->tail);
}

uint8_t cbuf_full(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return 2;
	}
	return (c->head + 1 == c->tail);
}

uint32_t cbuf_capacity(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return 0;
	}
	return c->max;
}

uint32_t cbuf_size(cbuf_handle_t c) {
	if (c == NULL) {
		LOG_ERROR("Parameters of circular buffer were not initialised!");
		return 0;
	}
	if (cbuf_empty(c) != 1)
		return c->max;
	return c->head >= c->tail ? c->head - c->tail : c->max + c->head - c->tail;
}