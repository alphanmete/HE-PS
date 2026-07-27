#ifdef USE_SEAL

#include "HEAggregatorCKKS.hpp"

#include <cmath>
#include <stdexcept>

static seal::SEALContext create_ckks_context() {
    seal::EncryptionParameters parms(seal::scheme_type::ckks);

    std::size_t poly_modulus_degree = 8192;

    parms.set_poly_modulus_degree(poly_modulus_degree);

    parms.set_coeff_modulus(
        seal::CoeffModulus::Create(
            poly_modulus_degree,
            {60, 40, 40, 60}
        )
    );

    return seal::SEALContext(parms);
}

HEAggregatorCKKS::HEAggregatorCKKS(std::size_t profile_size)
    : profile_size_(profile_size),
      scale_(std::pow(2.0, 40)),
      parms_(seal::scheme_type::ckks),
      context_(create_ckks_context()),
      secret_key_(),
      public_key_(),
      encryptor_(context_, [&]() {
          seal::KeyGenerator keygen(context_);

          secret_key_ = keygen.secret_key();

          keygen.create_public_key(public_key_);

          return public_key_;
      }()),
      evaluator_(context_),
      decryptor_(context_, secret_key_),
      encoder_(context_) {

    if (profile_size_ > encoder_.slot_count()) {
        throw std::runtime_error(
            "Profile size exceeds CKKS slot count."
        );
    }
}

seal::Ciphertext HEAggregatorCKKS::encrypt_profile(
    const std::vector<double>& profile
) {
    if (profile.size() != profile_size_) {
        throw std::runtime_error(
            "Profile has incorrect size."
        );
    }

    std::vector<double> slots(
        encoder_.slot_count(),
        0.0
    );

    for (std::size_t i = 0; i < profile.size(); ++i) {
        slots[i] = profile[i];
    }

    seal::Plaintext plain;

    encoder_.encode(
        slots,
        scale_,
        plain
    );

    seal::Ciphertext encrypted;

    encryptor_.encrypt(
        plain,
        encrypted
    );

    return encrypted;
}

void HEAggregatorCKKS::add_inplace(
    seal::Ciphertext& encrypted_total,
    const seal::Ciphertext& encrypted_update
) {
    evaluator_.add_inplace(
        encrypted_total,
        encrypted_update
    );
}

std::vector<double> HEAggregatorCKKS::decrypt_profile(
    const seal::Ciphertext& encrypted
) {
    seal::Plaintext plain;

    decryptor_.decrypt(
        encrypted,
        plain
    );

    std::vector<double> decoded;

    encoder_.decode(
        plain,
        decoded
    );

    std::vector<double> result(profile_size_);

    for (std::size_t i = 0; i < profile_size_; ++i) {
        result[i] = decoded[i];
    }

    return result;
}

#endif