/*
 * Copyright (c) 2026 Filip Kosecek
 *
 * Licensed under the MIT license.
 * See the LICENSE file in the project root for full license information.
 */

/**
 * @file nec.c
 *
 * @brief NEC protocol decoder.
 */

#include <stdint.h>

#include <hardware/sync.h>

#include "ir_driver.h"
#include "protocols/nec.h"

/**
 * PIO clock divider value. The sampling period is ~47 us.
 *
 * @note The rising/falling edges may cause metastability on the PIO hardware,
 *       which affects too many samples when the sampling frequency is
 *       high. In turn, the polarity of the affected samples is random and
 *       usually oscillates between logic levels which ruins the decoding
 *       process. This value was chosen and worked reliably during testing
 *       (at least for my hardware).
 */
#define CLKDIV               6240

/** NEC event durations. */
#define PREAMBLE_LOW_LENGTH  180
#define PREAMBLE_HIGH_LENGTH 90
#define PAUSE_LENGTH         11
#define PULSE_SHORT_LENGTH   (PAUSE_LENGTH)
#define PULSE_LONG_LENGTH    33

/** Deviation/error tolerance. */
#define DEVIATION_TOLERANCE  5

/** Calculate if the given value is within the tolerance. */
#define IS_WITHIN_DEVIATION_TOLERANCE(actual_val, exp_val, tolerance) \
        (((actual_val) >= (exp_val) - (tolerance)) && ((actual_val) <= (exp_val) + (tolerance)))

/** State of the packet processing used by the callback function. */
enum nec_state {
        NEC_STATE_IDLE,
        NEC_STATE_PREAMBLE_HIGH,
        NEC_STATE_DATA_LOW,
        NEC_STATE_DATA_HIGH
};

/** Raw packet data */
static volatile uint32_t nec_packet;

/** Data ready flag */
static volatile bool nec_packet_ready = false;

static spin_lock_t *nec_packet_lock;


/**
 * @internal
 *
 * @brief Verify the NEC packet.
 *
 * @param[in] packet Raw NEC packet.
 * @return Status.
 */
static bool nec_verify_packet(uint32_t packet)
{
        uint8_t addr, addr_inv, cmd, cmd_inv;

        addr = (uint8_t)packet;
        addr_inv = ~(uint8_t)(packet >> 8);
        cmd = (uint8_t)(packet >> 16);
        cmd_inv = ~(uint8_t)(packet >> 24);

        return (addr == addr_inv) && (cmd == cmd_inv);
}

/**
 * Retrieve the packet atomically from the IR driver and parse it. If there
 * is no packet available, the function blocks in a low power state and waits
 * for a signal from the IRQ thread.
 *
 * @param[out] nec_addr NEC address
 * @param[out] nec_cmd  NEC command
 *
 * @note This function is called from the worker thread (core 0).
 */
void nec_get_packet_data(uint8_t *nec_addr, uint8_t *nec_cmd)
{
        uint32_t saved_irq_flags;
        bool done = false;

        do {
                // Wait for event (interrupt or event signal from the other
                // core).
                __wfe();

                // Atomically fetch the packet and parse it.
                saved_irq_flags = spin_lock_blocking(nec_packet_lock);
                if (nec_packet_ready) {
                        done = true;
                        nec_packet_ready = false;
                        *nec_addr = (uint8_t)nec_packet;
                        *nec_cmd = (uint8_t)(nec_packet >> 16);
                }
                spin_unlock(nec_packet_lock, saved_irq_flags);
        } while (!done);
}

/**
 * @internal
 *
 * @brief Atomically set the raw packet.
 *
 * @param[in] packet Raw packet
 *
 * @note Executed from the IRQ thread (core 1).
 */
static void nec_set_packet_data(uint32_t packet)
{
        uint32_t saved_irq_flags;

        // Atomically set the packet data.
        saved_irq_flags = spin_lock_blocking(nec_packet_lock);
        nec_packet = packet;
        nec_packet_ready = true;
        spin_unlock(nec_packet_lock, saved_irq_flags);

        // Send event signal to the other core.
        __sev();
}

/**
 * @internal
 *
 * @brief Initialize the module's spin lock.
 */
static void nec_init(void)
{
        uint nec_packet_lock_num;

        // Initialize the data spin lock.
        nec_packet_lock_num = spin_lock_claim_unused(true);
        nec_packet_lock = spin_lock_init((uint)nec_packet_lock_num);
}

/**
 * @internal
 *
 * @brief Process sample callback.
 *
 * @note Executed from the IRQ context.
 */
static void nec_process_sample(uint32_t val, bool polarity)
{
        static enum nec_state nec_state = NEC_STATE_IDLE;
        static unsigned int bit_counter = 0;
        static uint32_t result = 0;

        switch (nec_state) {

        case NEC_STATE_IDLE:
                result = 0;
                bit_counter = 0;
                if (IS_WITHIN_DEVIATION_TOLERANCE(val, PREAMBLE_LOW_LENGTH, DEVIATION_TOLERANCE) && !polarity)
                        nec_state = NEC_STATE_PREAMBLE_HIGH;
                break;

        case NEC_STATE_PREAMBLE_HIGH:
                if (IS_WITHIN_DEVIATION_TOLERANCE(val, PREAMBLE_HIGH_LENGTH, DEVIATION_TOLERANCE) && polarity)
                        nec_state = NEC_STATE_DATA_LOW;
                else
                        nec_state = NEC_STATE_IDLE;
                break;

        case NEC_STATE_DATA_LOW:
                if (IS_WITHIN_DEVIATION_TOLERANCE(val, PAUSE_LENGTH, DEVIATION_TOLERANCE) && !polarity)
                        nec_state = NEC_STATE_DATA_HIGH;
                else
                        nec_state = NEC_STATE_IDLE;
                break;

        case NEC_STATE_DATA_HIGH:
                if (!polarity) {
                        nec_state = NEC_STATE_IDLE;
                        break;
                }

                // Check the pulse.
                result >>= 1;
                if (IS_WITHIN_DEVIATION_TOLERANCE(val, PULSE_LONG_LENGTH, DEVIATION_TOLERANCE)) {
                        result |= (1 << 31);
                } else if (!IS_WITHIN_DEVIATION_TOLERANCE(val, PULSE_SHORT_LENGTH, DEVIATION_TOLERANCE)) {
                        nec_state = NEC_STATE_IDLE;
                        break;
                }

                // Check if the full packet has been received.
                ++bit_counter;
                if (bit_counter < 32) {
                        nec_state = NEC_STATE_DATA_LOW;
                        break;
                }

                // Return the result.
                if (nec_verify_packet(result))
                        nec_set_packet_data(result);

                nec_state = NEC_STATE_IDLE;

                break;

        default:
                nec_state = NEC_STATE_IDLE;
                break;

        }
};

struct ir_decoder nec_decoder = {
        .clkdiv = CLKDIV / 6,
        .max_duration = PREAMBLE_LOW_LENGTH + DEVIATION_TOLERANCE,
        .decoder_init = nec_init,
        .process_sample = nec_process_sample
};
