#pragma once
#include <Arduino.h>

// Simple "every N ms" scheduler, similar to FastLED's CEveryNMilliseconds.
class EveryNMillis {
public:
    explicit EveryNMillis(uint32_t periodMs)
        : mPeriod(periodMs), mLast(0) {}

    // Returns true when the period has elapsed since the last "true".
    bool ready() {
        uint32_t now = millis();

        // Initialize on first use
        if (mLast == 0) {
            mLast = now;
            return false;  // first call: wait one full period
        }

        // Handle wrap-around via unsigned arithmetic
        if ((uint32_t)(now - mLast) >= mPeriod) {
            mLast = now;
            return true;
        }
        return false;
    }

    void reset() {
        mLast = millis();
    }

    void setPeriod(uint32_t periodMs) {
        mPeriod = periodMs;
        reset();
    }

private:
    uint32_t mPeriod;
    uint32_t mLast;
};

// Helper macros to create a unique static instance per call site
#define EVERYN_CONCAT_INNER(a, b) a##b
#define EVERYN_CONCAT(a, b) EVERYN_CONCAT_INNER(a, b)

// Run the following block every N milliseconds
#define EVERY_N_MILLISECONDS(N)                                           \
    static EveryNMillis EVERYN_CONCAT(_everyNMillis_, __LINE__)(N);      \
    if (EVERYN_CONCAT(_everyNMillis_, __LINE__).ready())

// Run the following block every N seconds
#define EVERY_N_SECONDS(N) EVERY_N_MILLISECONDS((N) * 1000UL)

#pragma once
#include <Arduino.h>

// Simple "every N ms" scheduler, similar to FastLED's CEveryNMilliseconds.
class EveryNMillis {
public:
    explicit EveryNMillis(uint32_t periodMs)
        : mPeriod(periodMs), mLast(0) {}

    // Returns true when the period has elapsed since the last "true".
    bool ready() {
        uint32_t now = millis();

        // Initialize on first use
        if (mLast == 0) {
            mLast = now;
            return false;  // first call: wait one full period
        }

        // Handle wrap-around via unsigned arithmetic
        if ((uint32_t)(now - mLast) >= mPeriod) {
            mLast = now;
            return true;
        }
        return false;
    }

    void reset() {
        mLast = millis();
    }

    void setPeriod(uint32_t periodMs) {
        mPeriod = periodMs;
        reset();
    }

private:
    uint32_t mPeriod;
    uint32_t mLast;
};

// Helper macros to create a unique static instance per call site
#define EVERYN_CONCAT_INNER(a, b) a##b
#define EVERYN_CONCAT(a, b) EVERYN_CONCAT_INNER(a, b)

// Run the following block every N milliseconds
#define EVERY_N_MILLISECONDS(N)                                           \
    static EveryNMillis EVERYN_CONCAT(_everyNMillis_, __LINE__)(N);      \
    if (EVERYN_CONCAT(_everyNMillis_, __LINE__).ready())

// Run the following block every N seconds
#define EVERY_N_SECONDS(N) EVERY_N_MILLISECONDS((N) * 1000UL)

