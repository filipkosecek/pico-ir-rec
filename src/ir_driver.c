/*
 * Copyright (c) 2026 Filip Kosecek
 *
 * Licensed under the MIT license.
 * See the LICENSE file in the project root for full license information.
 */

/**
 * @file ir_driver.c
 *
 * @brief IR driver implementation.
 *
 * This module implements the IR driver that runs in its separate IRQ thread
 * on core 1. Its job is to receive the pulse/pause sample from the PIO state
 * machine and pass it to the decoder's callback function. This architecture
 * allows to receive IR data even when the main application/core is busy
 * executing some application logic.
 *
 * The IR driver follows an event-driven model. The non-IRQ code just blocks
 * in a low-power sleep state (__wfi) waiting for an interrupt to fire,
 * consuming almost no power when there is no IR event.
 *
 * @note The decoder module is responsible for providing the interface
 *       for passing the data to the user application as the data format may
 *       vary across different protocols.
 */

#include <stdint.h>

#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>

#include "ir_driver.h"
#include "ir_decoder.h"
#include "ir_sampler.pio.h"

/** Currently active IR protocol decoder. */
static struct ir_decoder *active_ir_decoder;

/**
 * @brief IRQ handler
 *
 * @details The handler acknowledges the interrupt and drains the PIO's
 *          RX FIFO. Each sample is passed to the decoder callback function
 *          and it is up to the function to track the internal state
 *          of the data packet being received.
 *
 * @note This handler runs in the IRQ thread context (core 1).
 * @note It is the protocol's code job to pass the data to the user application
 *       running in the worker thread (core 0).
 */
static void irq_handler(void)
{
        while (!pio_sm_is_rx_fifo_empty(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX)) {
                uint32_t val;
                bool polarity;

                val = pio_sm_get(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX);

                polarity = val & (1u << 31);
                val &= ~(1u << 31);
                val = active_ir_decoder->max_duration - val;

                active_ir_decoder->process_sample(val, polarity);
        }

        irq_clear(__IR_DRIVER_CPU_IRQ_NUM);
}

/**
 * @brief IRQ thread entry function.
 *
 * @details The function enables interrupts on both CPU and PIO, enables the PIO
 *          state machine, and then waits for interrupts in a low-power sleep
 *          mode in an infinite loop.
 *
 * @note This function runs in the IRQ thread context (core 1).
 */
static void ir_irq_thread(void)
{
        // Install and enable the handler.
        irq_set_exclusive_handler(__IR_DRIVER_CPU_IRQ_NUM, irq_handler);
        irq_set_enabled(__IR_DRIVER_CPU_IRQ_NUM, true);

        // Enable interrupts on the PIO end.
        pio_set_irqn_source_enabled(IR_DRIVER_PIO_BANK, IR_DRIVER_PIO_IRQ_LINE, __IR_DRIVER_PIO_IRQ_SRC, true);

        // Enable the SM of the PIO instance.
        pio_sm_set_enabled(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, true);

        // Set the max pulse/pause duration.
        pio_sm_put(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, active_ir_decoder->max_duration);

        // Wait in a low-power mode.
        for (;;)
                __wfi();
}

/**
 * @brief Initialize hardware components and launch the IRQ thread.
 *
 * @param[in] idec The IR decoder to be used.
 *
 * @note The function runs in the worker thread (core 0).
 */
void ir_driver_init(struct ir_decoder *idec)
{
        uint sm_initial_pc_val;
        pio_sm_config config;

        // Save the IR structure pointer for the IRQ handler.
        active_ir_decoder = idec;

        // Install the PIO program.
        sm_initial_pc_val = pio_add_program(IR_DRIVER_PIO_BANK, &ir_sampler_program);

        // Get the config corresponding to the PIO program.
        config = ir_sampler_program_get_default_config(sm_initial_pc_val);

        // Reset the PIO instance's state machine.
        pio_sm_init(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, sm_initial_pc_val, &config);

        // Mux GPIOs to the PIO instance.
        pio_gpio_init(IR_DRIVER_PIO_BANK, IR_DRIVER_PIN);

        // Configure pins of the SM of the PIO instance.
        pio_sm_set_in_pins(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, IR_DRIVER_PIN);
        pio_sm_set_jmp_pin(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, IR_DRIVER_PIN);

#ifdef IR_DRIVER_INTERNAL_PULLUP
        // IR protocols' polarity is high when idle, therefore we need to enable
        // the pin's internal pull-up resistor.
        gpio_pull_up(IR_DRIVER_PIN);
#endif

        // Config the clock divider.
        pio_sm_set_clkdiv(IR_DRIVER_PIO_BANK, IR_DRIVER_SM_INDEX, active_ir_decoder->clkdiv);

        // Call the decoder init function if it is non-null.
        if (active_ir_decoder->decoder_init != NULL)
                active_ir_decoder->decoder_init();
}

/**
 * @brief Launch the IR sampling.
 *
 * @warning Function ir_driver_init must be called before calling this function.
 */
void ir_driver_run(void)
{
        // Launch the IRQ thread.
        multicore_reset_core1();
        multicore_launch_core1(ir_irq_thread);
}
