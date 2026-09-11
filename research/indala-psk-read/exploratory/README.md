Ad-hoc scripts as actually run during the 2026-09-10 session, kept for provenance.
Superseded by `../analyse.py`, which consolidates all of them into one pass.

    first-pass-analyse.py  basic stats + spectrum (its fc/2 band mask had an
                           off-by-one that excluded the Nyquist bin itself)
    peek.py                raw hex + ASCII waveform of a sample range
    overview.py            per-window peak-to-peak activity map across a capture
                           (this is what exposed the glitches in the baseline)
    clean.py               glitch-screened band-energy comparison
    psk.py                 coherent fc/2 PSK1 detector + frame autocorrelation

Note these were written against numpy 2.x, where `ndarray.ptp()` was removed in
favour of `np.ptp(arr)`.
