#pragma once

#ifdef USE_SEAL

#include <seal/seal.h>
#include <vector>
#include <cstddef>

class HEAggregatorCKKS {
public:
    explicit HEAggregatorCKKS(std::size_t profile_size);

    seal::Ciphertext encrypt_profile(const std::vector<double>& profile);

    void add_inplace(
        seal::Ciphertext& encrypted_total,
        const seal::Ciphertext& encrypted_update
    );

    std::vector<double> decrypt_profile(
        const seal::Ciphertext& encrypted
    );

private:
    std::size_t profile_size_;
    double scale_;

    seal::EncryptionParameters parms_;
    seal::SEALContext context_;

    seal::SecretKey secret_key_;
    seal::PublicKey public_key_;

    seal::Encryptor encryptor_;
    seal::Evaluator evaluator_;
    seal::Decryptor decryptor_;
    seal::CKKSEncoder encoder_;
};

#endif