# HE Profile Steering C++ Port

This is a C++17 port of the uploaded Python profile steering example, including a C++ implementation of the relevant `optAlg.py` buffer-planning functionality used by Battery, EV, and HeatPump.

## Build without SEAL

```bash
cmake -S . -B build
cmake --build build
./build/he_profile_steering
```

## Build with Microsoft SEAL

Install SEAL first, then:

```bash
cmake -S . -B build-seal -DUSE_SEAL=ON
cmake --build build-seal
./build-seal/he_profile_steering
```

## Notes

- The main algorithm mirrors `ProfileSteering.init` and `ProfileSteering.iterative`.
- The C++ `OptAlg` implements the active-power continuous buffer-planning path used by the uploaded Battery, HeatPump, and default continuous EV models.
- The SEAL module currently demonstrates BFV encrypted vector aggregation/decryption on the final rounded aggregate profile.
