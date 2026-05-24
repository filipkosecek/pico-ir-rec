/*
 * Copyright (c) 2026 Filip Kosecek
 *
 * Licensed under the MIT license.
 * See the LICENSE file in the project root for full license information.
 */

/**
 * @file ir_driver.h
 *
 * @brief IR driver settings.
 */

#ifndef IR_DRIVER_H
#define IR_DRIVER_H

#include <stdint.h>

#include <hardware/pio.h>

#include "ir_decoder.h"

/*
 * IR driver settings (defined in CMakeLists.txt)
 *
 * PIO bank to be used (pio0 or pio1):
 * IR_DRIVER_PIO_BANK
 *
 * Index of the state machine to be used (0-3):
 * IR_DRIVER_SM_INDEX
 *
 * PIO bank's IRQ line (0 or 1):
 * IR_DRIVER_PIO_IRQ_LINE
 *
 * Infrared receiver pin:
 * IR_DRIVER_PIN
 *
 * Enable the internal pull-up resistor on the IR receiver pin if defined:
 * IR_DRIVER_INTERNAL_PULLUP
 */

// Check IR_DRIVER_SM_INDEX value.
#if IR_DRIVER_SM_INDEX != 0 && IR_DRIVER_SM_INDEX != 1 && IR_DRIVER_SM_INDEX != 2 && IR_DRIVER_SM_INDEX != 3
#error "IR_DRIVER_SM_INDEX must be within range 0-3."
#endif

/**
 * @internal
 *
 * PIO bank's IRQ source (used in the PIO program).
 */
#define __IR_DRIVER_PIO_IRQ_SRC2(x) pis_sm ## x ## _rx_fifo_not_empty
#define __IR_DRIVER_PIO_IRQ_SRC1(x) __IR_DRIVER_PIO_IRQ_SRC2(x)
#define __IR_DRIVER_PIO_IRQ_SRC __IR_DRIVER_PIO_IRQ_SRC1(IR_DRIVER_SM_INDEX)

/**
 * @internal
 *
 * The CPU facing IRQ number.
 */
#define __IR_DRIVER_CPU_IRQ_NUM PIO_IRQ_NUM(IR_DRIVER_PIO_BANK, IR_DRIVER_PIO_IRQ_LINE)

/** IR driver init function. */
void ir_driver_init(struct ir_decoder *);

/** Launch the IR sampling code. */
void ir_driver_run(void);

#endif
