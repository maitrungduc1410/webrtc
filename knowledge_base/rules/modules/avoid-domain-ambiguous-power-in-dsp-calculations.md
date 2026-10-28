---
name: webrtc/modules/avoid-domain-ambiguous-power-in-dsp-calculations
description: In audio signal processing, name sum-of-squared samples using domain terms like energy, and avoid using power for mathematical exponents or denominators
rubric: readability
severity: should_fix
---

**Rule.** In audio and digital signal processing code, name variables
representing the sum of squared sample amplitudes using domain-standard terms
like `energy` (or `power` when normalized over time/samples) rather than literal
mathematical descriptions like `sum_of_squares`. Conversely, avoid using
domain-ambiguous names like `power` for mathematical exponentiation results or
denominator terms; prefer descriptive paired names (such as `parametric_denom`
paired with `parametric_num`).

**Why.** In audio processing modules, 'energy' and 'power' carry standard
physical meanings representing signal energy and average power. Using
domain-standard terms improves readability and conciseness across audio
pipelines, while avoiding 'power' for purely arithmetic exponents or divisors
prevents confusion with actual signal power measurements.

## Examples

Bad:

```cpp
// Literal mathematical description for signal energy:
double accumulated_input_sum_of_squares = 0.0;
accumulated_input_sum_of_squares += sample * sample;

// Using 'power' for mathematical exponentiation / denominator:
float power = PowApproximation(use_band, parametric_exp);
RTC_DCHECK_NE(power, 0.f);
parametric_noise_spectrum_[i] = parametric_num / power;
```

Good:

```cpp
// Domain-standard terminology for signal energy:
double accumulated_input_energy = 0.0;
accumulated_input_energy += sample * sample;

// Descriptive paired names for arithmetic denominator:
float parametric_denom = PowApproximation(use_band, parametric_exp);
RTC_DCHECK_NE(parametric_denom, 0.f);
parametric_noise_spectrum_[i] = parametric_num / parametric_denom;
```
