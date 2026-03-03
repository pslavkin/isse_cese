/**
 * @file morse.cpp
 * @brief Implementation of ASCII <-> Morse code converter for POSIX.
 *
 * Compile:
 * @code
 *   g++ -std=c++11 -Wall -o morse morse.cpp
 *   ./morse
 * @endcode
 *
 * Usage:
 *   - Press and hold SPACE to enter Morse code
 *   - Short press (<300 ms) = dot '.'
 *   - Long press  (>=300 ms) = dash '-'
 *   - Wait 1 second = symbol decoded and played on LED
 *   - Ctrl+C to quit
 */

#include "morse.hpp"

#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <termios.h>
#include <unistd.h>

// ─────────────────────────────────────────────────────────────────────────────
// POSIX terminal helpers (file-scope, not part of any class)
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Saved original terminal settings, restored on exit. */
static struct termios g_origTermios;

/**
 * @brief Switch terminal to raw non-blocking mode.
 *
 * Disables canonical mode (ICANON) and echo (ECHO).
 * Sets VMIN=0 and VTIME=0 so read() returns immediately.
 * Called once by App::init().
 */
static void termRaw() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &g_origTermios);
    raw = g_origTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;  ///< Return immediately even with 0 bytes read.
    raw.c_cc[VTIME] = 0;  ///< No timeout.
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/**
 * @brief Restore the original terminal settings.
 * Registered with atexit() by App::init().
 */
static void termRestore() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_origTermios);
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Returns milliseconds since an arbitrary fixed point (CLOCK_MONOTONIC).
 *
 * On embedded: replace with HAL_GetTick() or similar.
 *
 * @return Elapsed milliseconds as uint32_t.
 */
uint32_t Timer::millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Blocking sleep using nanosleep().
 *
 * On embedded: replace with HAL_Delay() or a busy-wait loop.
 *
 * @param ms Duration in milliseconds.
 */
void Timer::delay(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Button
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a Button.
 * @param pin GPIO pin (unused in POSIX simulation).
 */
Button::Button(uint8_t pin)
    : pin_(pin), state_(false), pressTime_(0) {}

/**
 * @brief Initialize button hardware.
 * On POSIX, terminal raw mode is handled by App::init().
 */
void Button::init() {
    // terminal set to raw mode by App::init()
}

/**
 * @brief Non-blocking poll: returns true while SPACE is held down.
 *
 * On first detection of SPACE, records pressTime_ for duration tracking.
 * On POSIX: reads one byte from stdin in non-blocking mode.
 * On embedded: replace with a GPIO_ReadPin() call.
 *
 * @return true if SPACE (simulated button) is currently pressed.
 */
bool Button::isPressed() {
    char c = 0;
    int n = read(STDIN_FILENO, &c, 1);  // non-blocking: returns 0 if no key

    if (n > 0 && c == ' ') {
        if (!state_) {
            // rising edge: record press start time
            state_     = true;
            pressTime_ = Timer::millis();
        }
        return true;
    }

    // falling edge or no key: button released
    if (state_) {
        state_ = false;
    }
    return false;
}

/**
 * @brief Calculate how long the button has been held.
 * @return Milliseconds since the last rising edge (press start).
 */
uint32_t Button::getDuration() {
    return Timer::millis() - pressTime_;
}

// ─────────────────────────────────────────────────────────────────────────────
// LED
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct an LED.
 * @param pin GPIO pin (unused in POSIX simulation).
 */
LED::LED(uint8_t pin) : pin_(pin) {}

/**
 * @brief Initialize LED hardware.
 * On embedded: configure GPIO pin as push-pull output.
 */
void LED::init() {}

/**
 * @brief Turn LED on.
 * On POSIX: prints [*] LED ON to stdout.
 * On embedded: set GPIO pin HIGH.
 */
void LED::on() {
    printf("\r[*] LED ON  ");
    fflush(stdout);
}

/**
 * @brief Turn LED off.
 * On POSIX: prints [ ] LED OFF to stdout.
 * On embedded: set GPIO pin LOW.
 */
void LED::off() {
    printf("\r[ ] LED OFF ");
    fflush(stdout);
}

/**
 * @brief Blink the LED: turn on for @p duration ms, then off.
 * Adds a 50 ms trailing gap to separate consecutive blinks visually.
 * @param duration Time in milliseconds to keep LED on.
 */
void LED::blink(uint32_t duration) {
    on();
    Timer::delay(duration);
    off();
    Timer::delay(50);  // brief inter-blink gap
}

// ─────────────────────────────────────────────────────────────────────────────
// MorseTable
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief International Morse Code table.
 *
 * Layout:
 *   - Indices  0-25 → letters A-Z
 *   - Indices 26-35 → digits  0-9
 */
const char* MorseTable::TABLE[36] = {
    // A       B       C       D       E      F       G       H       I
    ".-",   "-...", "-.-.", "-..",  ".",   "..-.", "--.",  "....", "..",
    // J       K       L       M      N      O       P       Q       R
    ".---", "-.-",  ".-..", "--",  "-.",  "---",  ".--.", "--.-", ".-.",
    // S      T      U      V       W      X       Y       Z
    "...",  "-",   "..-", "...-", ".--", "-..-", "-.--", "--..",
    // 0        1        2        3        4
    "-----", ".----", "..---", "...--", "....-",
    // 5        6        7        8        9
    ".....", "-....", "--...", "---..", "----."
};

/** @brief Default constructor — table is static, nothing to initialize. */
MorseTable::MorseTable() {}

/**
 * @brief Look up the Morse string for an ASCII character.
 *
 * Converts lowercase to uppercase before lookup.
 *
 * @param c Character to look up (A-Z, a-z, 0-9).
 * @return Pointer to Morse string, or nullptr if not in table.
 */
const char* MorseTable::asciiToMorse(char c) const {
    if (c >= 'a' && c <= 'z') c -= 32;         // normalize to uppercase
    if (c >= 'A' && c <= 'Z') return TABLE[c - 'A'];
    if (c >= '0' && c <= '9') return TABLE[26 + (c - '0')];
    return nullptr;
}

/**
 * @brief Reverse lookup: find the ASCII character for a Morse string.
 *
 * Performs a linear scan of the TABLE array.
 *
 * @param code Null-terminated Morse string (e.g. ".-").
 * @return Matching ASCII character, or '?' if not found.
 */
char MorseTable::morseToAscii(const char* code) const {
    for (int i = 0; i < 26; i++) {
        if (strcmp(TABLE[i], code) == 0)
            return 'A' + i;
    }
    for (int i = 0; i < 10; i++) {
        if (strcmp(TABLE[26 + i], code) == 0)
            return '0' + i;
    }
    return '?';
}

// ─────────────────────────────────────────────────────────────────────────────
// MorseEncoder
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a MorseEncoder.
 * @param button Pointer to the Button to poll.
 * @param table  Pointer to the MorseTable for symbol decoding.
 */
MorseEncoder::MorseEncoder(Button* button, MorseTable* table)
    : button_(button)
    , table_(table)
    , bufIdx_(0)
    , lastPressed_(false)
    , releaseTime_(0)
{
    memset(buffer_, 0, sizeof(buffer_));
}

/**
 * @brief Classify a press as dot or dash based on duration.
 * @param duration Press duration in milliseconds.
 * @return '.' if shorter than DOT_DASH_THRESHOLD_MS, '-' otherwise.
 */
char MorseEncoder::classifyPress(uint32_t duration) {
    return (duration < Button::DOT_DASH_THRESHOLD_MS) ? '.' : '-';
}

/**
 * @brief Zero out the dot/dash buffer and reset the write index.
 * Called after a symbol has been successfully decoded.
 */
void MorseEncoder::resetBuffer() {
    memset(buffer_, 0, sizeof(buffer_));
    bufIdx_ = 0;
}

/**
 * @brief Poll the button and append dot/dash on falling edge.
 *
 * Detects the falling edge (button just released), measures
 * press duration, classifies it, and appends to buffer_.
 * Prints press feedback to stdout for debugging.
 *
 * Must be called every loop iteration.
 */
void MorseEncoder::update() {
    bool pressed = button_->isPressed();

    // falling edge: button was pressed last tick, now released
    if (lastPressed_ && !pressed) {
        uint32_t dur = button_->getDuration();
        if (bufIdx_ < 7) {
            buffer_[bufIdx_++] = classifyPress(dur);
            buffer_[bufIdx_]   = '\0';
        }
        releaseTime_ = Timer::millis();
        printf("\n  press: %c  buffer: %s\n", buffer_[bufIdx_-1], buffer_);
        fflush(stdout);
    }

    lastPressed_ = pressed;
}

/**
 * @brief Decode and return the buffered symbol after silence timeout.
 *
 * Waits SYMBOL_TIMEOUT_MS of silence after the last button release
 * before decoding. Resets the buffer after decoding to prepare
 * for the next symbol.
 *
 * @return Decoded ASCII character, or '\\0' if not yet ready.
 */
char MorseEncoder::getSymbol() {
    if (bufIdx_ > 0 && !lastPressed_) {
        uint32_t silence = Timer::millis() - releaseTime_;
        if (silence >= Button::SYMBOL_TIMEOUT_MS) {
            char decoded = table_->morseToAscii(buffer_);
            resetBuffer();
            releaseTime_ = Timer::millis();  // prevent immediate re-decode
            return decoded;
        }
    }
    return '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// MorseDecoder
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct a MorseDecoder.
 * @param led   Pointer to the LED to drive.
 * @param table Pointer to the MorseTable for ASCII-to-Morse lookup.
 */
MorseDecoder::MorseDecoder(LED* led, MorseTable* table)
    : led_(led)
    , table_(table)
    , pos_(0)
    , nextTime_(0)
    , ledOn_(false)
{
    memset(input_, 0, sizeof(input_));
}

/**
 * @brief Load a new ASCII character and start LED playback.
 *
 * Looks up the Morse code string and resets pos_ to 0
 * so update() starts from the first symbol.
 *
 * @param c ASCII character to convert and play (A-Z, 0-9).
 */
void MorseDecoder::decode(char c) {
    const char* morse = table_->asciiToMorse(c);
    if (morse) {
        strncpy(input_, morse, sizeof(input_) - 1);
        input_[sizeof(input_)-1] = '\0';
        pos_      = 0;
        nextTime_ = Timer::millis();
        printf("\n  decoding '%c' -> %s\n", c, input_);
        fflush(stdout);
    }
}

/**
 * @brief Blink LED for DOT_MS milliseconds (short blink).
 * Delegates to LED::blink().
 */
void MorseDecoder::playDot() {
    led_->blink(DOT_MS);
}

/**
 * @brief Blink LED for DASH_MS milliseconds (long blink).
 * Delegates to LED::blink().
 */
void MorseDecoder::playDash() {
    led_->blink(DASH_MS);
}

/**
 * @brief Turn LED off and wait GAP_MS milliseconds.
 * Used after the last symbol of a character.
 */
void MorseDecoder::playWordGap() {
    led_->off();
    Timer::delay(GAP_MS);
}

/**
 * @brief Advance LED playback by one symbol step (non-blocking).
 *
 * Checks if the next blink is due based on nextTime_.
 * Plays one dot or dash per call, then schedules the next step.
 * After the last symbol, plays a word gap.
 *
 * Must be called every loop iteration.
 */
void MorseDecoder::update() {
    if (pos_ >= strlen(input_)) return;       // nothing queued
    if (Timer::millis() < nextTime_) return;  // not yet time for next symbol

    char sym = input_[pos_++];
    if (sym == '.') {
        playDot();
        nextTime_ = Timer::millis() + DOT_MS + GAP_MS;
    } else if (sym == '-') {
        playDash();
        nextTime_ = Timer::millis() + DASH_MS + GAP_MS;
    }

    // after last symbol in the sequence, play inter-character gap
    if (pos_ >= strlen(input_)) {
        playWordGap();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Construct App and wire all subsystems.
 *
 * Button on pin 0, LED on pin 1.
 * MorseEncoder receives pointer to button_ and table_.
 * MorseDecoder receives pointer to led_ and table_.
 */
App::App()
    : button_(0)
    , led_(1)
    , table_()
    , encoder_(&button_, &table_)
    , decoder_(&led_,    &table_)
{}

/**
 * @brief Initialize all subsystems and set up the terminal.
 *
 * - Calls button_.init() and led_.init()
 * - Sets terminal to raw mode via termRaw()
 * - Registers termRestore() with atexit()
 * - Prints usage instructions
 */
void App::init() {
    button_.init();
    led_.init();
    termRaw();
    atexit(termRestore);

    printf("=== ASCII <-> Morse POSIX Simulator ===\n");
    printf("  ENCODER: press SPACE (short=dot, long=dash)\n");
    printf("           silence 1s = decode symbol\n");
    printf("  DECODER: encoded char is played on LED\n");
    printf("  Ctrl+C to quit\n\n");
    fflush(stdout);
}

/**
 * @brief Main super-loop. Runs forever until process is killed.
 *
 * Each 10 ms iteration:
 *   1. encoder_.update()    — detect button edges, build dot/dash buffer
 *   2. encoder_.getSymbol() — decode buffer to ASCII after silence timeout
 *   3. decoder_.decode()    — if a new char arrived, load it into the decoder
 *   4. decoder_.update()    — advance LED blink playback by one step
 */
void App::run() {
    while (true) {
        encoder_.update();

        char sym = encoder_.getSymbol();
        if (sym != '\0' && sym != '?') {
            printf("\n>>> decoded: '%c'\n", sym);
            fflush(stdout);
            decoder_.decode(sym);
        }

        decoder_.update();

        Timer::delay(10);  // 10 ms main loop tick
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Program entry point.
 *
 * Instantiates App, calls init() and run().
 * run() never returns under normal operation.
 *
 * @return 0 (never reached).
 */
int main() {
    App app;
    app.init();
    app.run();
    return 0;
}

