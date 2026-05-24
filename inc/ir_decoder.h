/*
 * Copyright (c) 2026 Filip Kosecek
 *
 * Licensed under the MIT license.
 * See the LICENSE file in the project root for full license information.
 */

/**
 * @file ir_decoder.h
 *
 * IR decoder structure.
 */

#ifndef IR_DECODER_H
#define IR_DECODER_H

#include <stdint.h>

struct ir_decoder {
        /** Clock divider setting for the PIO state machine for this protocol. */
        float clkdiv;

        /** Maximum duration of a pulse/pause for this protocol. */
        uint32_t max_duration;

        /** Decoder init function called from the IR driver init function. */
        void (*decoder_init) (void);

        /**
         * Callback function whose job is to process a sample passed
         * from the PIO state machine.
         *
         * @warning Called from the IRQ context, so this function must not call
         *          any blocking primitives. Use spin locks for synchronization.
         */
        void (*process_sample) (uint32_t, bool);
};

#endif
