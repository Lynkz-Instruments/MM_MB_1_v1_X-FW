#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

__attribute__((nonnull(1,3,4)))
uint8_t hecc_encode(const uint8_t *inbuf, size_t insize, uint8_t *outbuf, size_t *outsize);

__attribute__((nonnull(1,3,4)))
uint8_t hecc_decode(const uint8_t *inbuf, size_t insize, uint8_t *outbuf, size_t *outsize);
