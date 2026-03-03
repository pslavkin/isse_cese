/**
 * @file morse.hpp
 * @brief ASCII <-> Morse code converter for POSIX simulation.
 *
 * Simulates an embedded system with:
 *   - One button (SPACE key) as input for the encoder
 *   - One LED (stdout) as output for the decoder
 *
 * Architecture follows the class diagram:
 *   App owns MorseEncoder and MorseDecoder.
 *   MorseEncoder polls Button and queries MorseTable.
 *   MorseDecoder drives LED and queries MorseTable.
 *   Both use Timer for timing.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Static timing utility wrapping POSIX clock_gettime.
 *
 * No instance needed. All methods are static.
 * On embedded targets, replace with HAL_GetTick() / HAL_Delay().
 */
class Timer {
public:
    /**
     * @brief Returns milliseconds elapsed since program start.
     * @return Elapsed time in milliseconds as uint32_t.
     */
    static uint32_t millis();

    /**
     * @brief Blocking delay.
     * @param ms Duration to wait in milliseconds.
     */
    static void delay(uint32_t ms);
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Represents a physical push button.
 *
 * On POSIX, simulated by the SPACE key in raw terminal mode.
 * On embedded, replace isPressed() with a GPIO read.
 *
 * Timing constants determine dot/dash classification:
 * - Press shorter than DOT_DASH_THRESHOLD_MS  → dot '.'
 * - Press equal or longer                     → dash '-'
 * - Silence longer than SYMBOL_TIMEOUT_MS     → end of symbol
 * - Silence longer than WORD_TIMEOUT_MS       → end of word
 */
class Button {
public:
    /** @brief Press duration threshold in ms to distinguish dot from dash. */
    static constexpr uint32_t DOT_DASH_THRESHOLD_MS = 300;

    /** @brief Silence duration in ms after which a symbol is considered complete. */
    static constexpr uint32_t SYMBOL_TIMEOUT_MS = 1000;

    /** @brief Silence duration in ms after which a word gap is inserted. */
    static constexpr uint32_t WORD_TIMEOUT_MS = 2000;

    /**
     * @brief Construct a Button on a given pin.
     * @param pin GPIO pin number (unused in POSIX simulation).
     */
    explicit Button(uint8_t pin);

    /**
     * @brief Initialize the button hardware.
     * On POSIX, terminal raw mode is set by App::init().
     */
    void init();

    /**
     * @brief Poll the button state.
     * @return true if button is currently pressed, false otherwise.
     */
    bool isPressed();

    /**
     * @brief Get how long the button has been held since last press.
     * @return Duration in milliseconds since pressTime_.
     */
    uint32_t getDuration();

private:
    uint8_t  pin_;        ///< GPIO pin number.
    bool     state_;      ///< Current pressed state.
    uint32_t pressTime_;  ///< Timestamp of the last press event.
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Represents an LED output.
 *
 * On POSIX, simulated by printing symbols to stdout:
 *   - [*] = LED ON
 *   - [ ] = LED OFF
 *
 * On embedded, replace on()/off() with GPIO writes.
 */
class LED {
public:
    /**
     * @brief Construct an LED on a given pin.
     * @param pin GPIO pin number (unused in POSIX simulation).
     */
    explicit LED(uint8_t pin);

    /**
     * @brief Initialize the LED hardware.
     * On embedded, configure the GPIO pin as output.
     */
    void init();

    /** @brief Turn the LED on (set GPIO HIGH). */
    void on();

    /** @brief Turn the LED off (set GPIO LOW). */
    void off();

    /**
     * @brief Blink the LED for a given duration then turn off.
     * @param duration Time in milliseconds to keep LED on.
     */
    void blink(uint32_t duration);

private:
    uint8_t pin_;  ///< GPIO pin number.
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief International Morse Code lookup table.
 *
 * Covers A-Z (indices 0-25) and 0-9 (indices 26-35).
 * Each entry is a null-terminated string of '.' and '-' characters.
 *
 * Example:
 * @code
 *   MorseTable t;
 *   t.asciiToMorse('A');  // returns ".-"
 *   t.morseToAscii(".-"); // returns 'A'
 * @endcode
 */
class MorseTable {
public:
    /** @brief Construct and initialize the lookup table. */
    MorseTable();

    /**
     * @brief Convert an ASCII character to its Morse code string.
     * @param c Input character (A-Z, a-z, 0-9). Case-insensitive.
     * @return Pointer to a null-terminated Morse string (e.g. ".-"),
     *         or nullptr if character is not in the table.
     */
    const char* asciiToMorse(char c) const;

    /**
     * @brief Convert a Morse code string to its ASCII character.
     * @param code Null-terminated Morse string (e.g. ".-").
     * @return Corresponding ASCII character, or '?' if not found.
     */
    char morseToAscii(const char* code) const;

private:
    /** @brief Static lookup table: indices 0-25 = A-Z, 26-35 = 0-9. */
    static const char* TABLE[36];
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Encodes button presses into ASCII characters via Morse code.
 *
 * Reads button events, classifies each press as dot or dash,
 * accumulates them in a buffer, and decodes the buffer to ASCII
 * after a silence timeout.
 *
 * Call update() every loop tick to poll the button.
 * Call getSymbol() every loop tick to retrieve a decoded character.
 *
 * @see Button
 * @see MorseTable
 * @see Timer
 */
class MorseEncoder {
public:
    /**
     * @brief Construct a MorseEncoder.
     * @param button Pointer to the Button instance to read from.
     * @param table  Pointer to the MorseTable instance for decoding.
     */
    MorseEncoder(Button* button, MorseTable* table);

    /**
     * @brief Poll the button and update the internal dot/dash buffer.
     * Must be called every loop iteration (e.g. every 10 ms).
     */
    void update();

    /**
     * @brief Retrieve the last decoded ASCII character if ready.
     *
     * A character is ready after SYMBOL_TIMEOUT_MS of silence
     * following the last button release.
     *
     * @return Decoded ASCII character, or '\\0' if not yet ready.
     */
    char getSymbol();

private:
    Button*     button_;       ///< Pointer to the hardware button.
    MorseTable* table_;        ///< Pointer to the Morse lookup table.
    char        buffer_[8];    ///< Accumulated dot/dash string for current symbol.
    uint8_t     bufIdx_;       ///< Next write index in buffer_.
    bool        lastPressed_;  ///< Button state in the previous tick.
    uint32_t    releaseTime_;  ///< Timestamp of the last button release.

    /**
     * @brief Classify a press duration as dot or dash.
     * @param duration Press duration in milliseconds.
     * @return '.' for short press, '-' for long press.
     */
    char classifyPress(uint32_t duration);

    /** @brief Clear the dot/dash buffer and reset the index. */
    void resetBuffer();
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Decodes ASCII characters into Morse code LED blink sequences.
 *
 * Accepts an ASCII character via decode(), looks up its Morse code,
 * and plays the sequence on the LED non-blocking through update().
 *
 * Call decode() when a new character arrives.
 * Call update() every loop tick to advance LED playback.
 *
 * @see LED
 * @see MorseTable
 * @see Timer
 */
class MorseDecoder {
public:
    /** @brief Duration in ms of a dot blink. */
    static constexpr uint32_t DOT_MS  = 200;

    /** @brief Duration in ms of a dash blink. */
    static constexpr uint32_t DASH_MS = 600;

    /** @brief Duration in ms of the gap between symbols. */
    static constexpr uint32_t GAP_MS  = 200;

    /**
     * @brief Construct a MorseDecoder.
     * @param led   Pointer to the LED instance to drive.
     * @param table Pointer to the MorseTable instance for encoding.
     */
    MorseDecoder(LED* led, MorseTable* table);

    /**
     * @brief Load an ASCII character and start LED playback.
     *
     * Looks up the Morse code for @p c and resets playback to
     * the beginning of the sequence.
     *
     * @param c ASCII character to decode and play (A-Z, 0-9).
     */
    void decode(char c);

    /**
     * @brief Advance the LED blink sequence by one step.
     *
     * Non-blocking: returns immediately if the next blink
     * is not yet due. Must be called every loop iteration.
     */
    void update();

private:
    LED*        led_;       ///< Pointer to the hardware LED.
    MorseTable* table_;     ///< Pointer to the Morse lookup table.
    char        input_[8];  ///< Current Morse string being played (e.g. ".-").
    uint8_t     pos_;       ///< Current position in input_.
    uint32_t    nextTime_;  ///< Timestamp when the next blink step is due.
    bool        ledOn_;     ///< Tracks current LED state.

    /** @brief Blink the LED for DOT_MS milliseconds. */
    void playDot();

    /** @brief Blink the LED for DASH_MS milliseconds. */
    void playDash();

    /** @brief Turn off the LED and wait GAP_MS milliseconds. */
    void playWordGap();
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Top-level application class.
 *
 * Owns all subsystem instances by value (composition).
 * Wires Button → MorseEncoder → MorseDecoder → LED.
 * Runs a blocking super-loop calling update() on each subsystem.
 *
 * @code
 *   App app;
 *   app.init();
 *   app.run();  // never returns
 * @endcode
 */
class App {
public:
    /** @brief Construct App and wire all subsystems together. */
    App();

    /**
     * @brief Initialize all hardware and print usage instructions.
     * Sets terminal to raw mode on POSIX.
     */
    void init();

    /**
     * @brief Run the main super-loop forever.
     *
     * Each iteration (10 ms tick):
     *   1. encoder_.update()    — poll button
     *   2. encoder_.getSymbol() — check for decoded char
     *   3. decoder_.decode()    — feed char to LED decoder if ready
     *   4. decoder_.update()    — advance LED blink sequence
     */
    void run();

private:
    Button       button_;   ///< Physical button (SPACE key on POSIX).
    LED          led_;      ///< Physical LED (stdout on POSIX).
    MorseTable   table_;    ///< Shared Morse code lookup table.
    MorseEncoder encoder_;  ///< Button → Morse → ASCII encoder.
    MorseDecoder decoder_;  ///< ASCII → Morse → LED decoder.
};
