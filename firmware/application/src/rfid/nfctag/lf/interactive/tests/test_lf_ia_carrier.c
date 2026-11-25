// Host test for lf_ia_carrier: the reader's carrier from synthetic field-line samples at 100 kHz.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../lf_ia_carrier.h"

static int m_failed = 0;
static int m_checks = 0;
static double m_worst_pct = 0.0;

#define CHECK(cond, ...)                                \
    do {                                                \
        m_checks++;                                     \
        if (!(cond)) {                                  \
            m_failed++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static uint32_t m_rng = 0x2468ACE1u;
static double noise(double sigma) {
    // Sum of uniforms, close enough to Gaussian here.
    double s = 0.0;
    for (int i = 0; i < 12; i++) {
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        s += (double)(m_rng & 0xFFFF) / 65535.0;
    }
    return (s - 6.0) * sigma;
}

// The field line with the load on: a level, the carrier's ripple with a third and fifth harmonic, and noise.
static void synth(int16_t *x, size_t n, double fc, double amp, double sigma, double phase) {
    const double fs = 100000.0;
    for (size_t i = 0; i < n; i++) {
        const double t = (double)i / fs;
        double v = 3900.0 + amp * sin(2 * M_PI * fc * t + phase) + amp / 3 * sin(2 * M_PI * 3 * fc * t + 2 * phase) +
                   amp / 5 * sin(2 * M_PI * 5 * fc * t + 3 * phase) + noise(sigma);
        x[i] = (int16_t)lrint(v);
    }
}

int main(void) {
    static int16_t x[512];
    lf_ia_carrier_t c;
    const double carriers[] = {118000, 120500, 122000, 124000, 124983.5, 125000, 125500, 126500, 129000, 131000};
    const size_t lengths[] = {96, 128, 200, 256, 512};

    for (size_t ci = 0; ci < sizeof(carriers) / sizeof(carriers[0]); ci++) {
        for (size_t li = 0; li < sizeof(lengths) / sizeof(lengths[0]); li++) {
            for (int trial = 0; trial < 5; trial++) {
                synth(x, lengths[li], carriers[ci], 700.0, 60.0, trial * 1.3);
                lf_ia_carrier_estimate(x, lengths[li], 100000.0f, 115000.0f, 135000.0f, 125000.0f, &c);
                const double pct = 100.0 * fabs(c.carrier_hz - carriers[ci]) / carriers[ci];
                if (pct > m_worst_pct) {
                    m_worst_pct = pct;
                }
                // A tenth of a percent is the scale that matters for a tag's timing.
                CHECK(c.status == LF_IA_CARRIER_OK && pct < 0.1, "fc %.1f n %zu: %d %.1f (%.3f %%) snr %.1f",
                      carriers[ci], lengths[li], c.status, c.carrier_hz, pct, c.snr_db);
            }
        }
    }

    // Noise alone has no clear line.
    for (size_t i = 0; i < 256; i++) {
        x[i] = (int16_t)lrint(3900.0 + noise(60.0));
    }
    lf_ia_carrier_estimate(x, 256, 100000.0f, 115000.0f, 135000.0f, 125000.0f, &c);
    CHECK(c.status == LF_IA_CARRIER_WEAK && c.carrier_hz == 125000.0f, "noise: %d snr %.1f", c.status, c.snr_db);

    // Too few samples.
    lf_ia_carrier_estimate(x, 40, 100000.0f, 115000.0f, 135000.0f, 125000.0f, &c);
    CHECK(c.status == LF_IA_CARRIER_TOO_FEW && c.carrier_hz == 125000.0f, "too few: %d", c.status);

    printf("%d checks, %d failed, worst error %.4f %%\n", m_checks, m_failed, m_worst_pct);
    return m_failed ? 1 : 0;
}
