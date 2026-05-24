# pico-ir-rec
A simple IR receiver library for Raspberry Pi Pico.

## Supported protocols
Currently, NEC is the only supported protocol. The library design allows
to easily add new protocol decoders.

## Contributing
Feel free to contribute to improve the existing code or add new protocol
decoders.

To add a new protocol decoder, implement the [decoder structure](inc/ir_decoder.h).
Additionally, create a header file named `<protocol>.h` under `inc/protocols`
and source file `<protocol>.c` under `src/protocols`. The header file must also
contain:
```
extern struct ir_decoder <protocol>_decoder;
```

You can take inspiration from the NEC protocol decoder implementation.

## Usage
The only dependency (aside from the C library) is
[Pico C SDK](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html).

To use the library, clone the repository or add it as a submodule to your
project. Then include the library subdirectory in your main CMakeLists file:
```
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/pico-ir-rec)
```

The default pico-ir-rec settings can be overridden from your project's
CMakeLists file by settings CMake variables before calling `add_subdirectory`.
All available settings overrides are:
```
set(IR_DRIVER_PIO_BANK        <pio bank>)   # PIO bank (pio0 or pio1)
set(IR_DRIVER_SM_INDEX        <sm index>)   # SM index (0-3)
set(IR_DRIVER_PIN             <pin num> )   # Pin to be used
set(IR_DRIVER_INTERNAL_PULLUP <*>)          # Enable the internal pull-up resistor on the input pin
```
