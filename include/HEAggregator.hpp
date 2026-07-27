#pragma once

#include "VectorUtils.hpp"

#ifdef USE_SEAL

#include "seal/seal.h"

#include <memory>

class HEAggregator {
public:
    explicit HEAggregator(
        size_t intervals,
        size_t poly_modulus_degree = 8192,
        int plain_modulus_bits = 30
    );

    seal::Ciphertext encrypt_profile(const Profile& profile);

    void add_inplace(
        seal::Ciphertext& encrypted_total,
        const seal::Ciphertext& encrypted_update
    );

    Profile decrypt_profile(const seal::Ciphertext& encrypted_profile);

    seal::Ciphertext encrypted_zero();

private:
    size_t intervals_;

    seal::EncryptionParameters parms_;
    std::shared_ptr<seal::SEALContext> context_;

    seal::PublicKey public_key_;
    seal::SecretKey secret_key_;

    std::unique_ptr<seal::BatchEncoder> encoder_;
    std::unique_ptr<seal::Encryptor> encryptor_;
    std::unique_ptr<seal::Evaluator> evaluator_;
    std::unique_ptr<seal::Decryptor> decryptor_;

    uint64_t plain_modulus_value_ = 0;
};

#endif