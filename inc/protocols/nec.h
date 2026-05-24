/*
 * Copyright (c) 2026 Filip Kosecek
 *
 * Licensed under the MIT license.
 * See the LICENSE file in the project root for full license information.
 */

/** @file nec.h */

#ifndef NEC_H
#define NEC_H

#include <stdint.h>

#include "ir_driver.h"

extern struct ir_decoder nec_decoder;

void nec_get_packet_data(uint8_t *, uint8_t *);

#endif
