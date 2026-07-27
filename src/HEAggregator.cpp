#ifdef USE_SEAL

#include "HEAggregator.hpp"

#include <cmath>
#include <stdexcept>

HEAggregator::HEAggregator(
    size_t intervals,
    size_t poly_modulus_degree,
    int plain_modulus_bits
)
    : intervals_(intervals),
      parms_(seal::scheme_type::bfv) {

    parms_.set_poly_modulus_degree(poly_modulus_degree);

    parms_.set_coeff_modulus(
        seal::CoeffModulus::BFVDefault(poly_modulus_degree)
    );

    parms_.set_plain_modulus(
        seal::PlainModulus::Batching(
            poly_modulus_degree,
            plain_modulus_bits
        )
    );

    context_ = std::make_shared<seal::SEALContext>(parms_);

    seal::KeyGenerator keygen(*context_);

    secret_key_ = keygen.secret_key();

    keygen.create_public_key(public_key_);

    encoder_ = std::make_unique<seal::BatchEncoder>(*context_);

    encryptor_ = std::make_unique<seal::Encryptor>(
        *context_,
        public_key_
    );

    evaluator_ = std::make_unique<seal::Evaluator>(*context_);

    decryptor_ = std::make_unique<seal::Decryptor>(
        *context_,
        secret_key_
    );

    plain_modulus_value_ = parms_.plain_modulus().value();

    if (intervals_ > encoder_->slot_count()) {
        throw std::runtime_error("Not enough BFV batching slots.");
    }
}

seal::Ciphertext HEAggregator::encrypt_profile(const Profile& profile) {
    if (profile.size() != intervals_) {
        throw std::runtime_error("Profile has incorrect size.");
    }

    std::vector<uint64_t> slots(
        encoder_->slot_count(),
        0ULL
    );

    const long long mod =
        static_cast<long long>(plain_modulus_value_);

    for (size_t i = 0; i < profile.size(); ++i) {
        long long rounded =
            static_cast<long long>(std::llround(profile[i]));

        slots[i] = static_cast<uint64_t>(
            ((rounded % mod) + mod) % mod
        );
    }

    seal::Plaintext plain;

    encoder_->encode(
        slots,
        plain
    );

    seal::Ciphertext encrypted;

    encryptor_->encrypt(
        plain,
        encrypted
    );

    return encrypted;
}

void HEAggregator::add_inplace(
    seal::Ciphertext& encrypted_total,
    const seal::Ciphertext& encrypted_update
) {
    evaluator_->add_inplace(
        encrypted_total,
        encrypted_update
    );
}

Profile HEAggregator::decrypt_profile(
    const seal::Ciphertext& encrypted_profile
) {
    seal::Plaintext plain;

    decryptor_->decrypt(
        encrypted_profile,
        plain
    );

    std::vector<uint64_t> slots;

    encoder_->decode(
        plain,
        slots
    );

    Profile out(intervals_);

    const uint64_t half = plain_modulus_value_ / 2;

    for (size_t i = 0; i < intervals_; ++i) {
        uint64_t v = slots[i];

        if (v > half) {
            out[i] = -static_cast<double>(
                plain_modulus_value_ - v
            );
        } else {
            out[i] = static_cast<double>(v);
        }
    }

    return out;
}

seal::Ciphertext HEAggregator::encrypted_zero() {
    return encrypt_profile(Profile(intervals_, 0.0));
}

#endif