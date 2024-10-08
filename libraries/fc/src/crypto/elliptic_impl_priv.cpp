#include <fc/fwd_impl.hpp>

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include "_elliptic_impl_priv.hpp"

#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/ec.h>

/* used by mixed + secp256k1 */

namespace fc { namespace ecc {
    namespace detail {

        private_key_impl::private_key_impl() BOOST_NOEXCEPT
        {
            _init_lib();
        }

        private_key_impl::private_key_impl( const private_key_impl& cpy ) BOOST_NOEXCEPT
        {
            _init_lib();
            this->_key = cpy._key;
        }

        private_key_impl& private_key_impl::operator=( const private_key_impl& pk ) BOOST_NOEXCEPT
        {
            _key = pk._key;
            return *this;
        }
    }

    static const private_key_secret empty_priv;

    private_key::private_key() {}

    private_key::private_key( const private_key& pk ) : my( pk.my ) {}

    private_key::private_key( private_key&& pk ) : my( std::move( pk.my ) ) {}

    private_key::~private_key() {}

    private_key& private_key::operator=( private_key&& pk )
    {
        my = std::move( pk.my );
        return *this;
    }

    private_key& private_key::operator=( const private_key& pk )
    {
        my = pk.my;
        return *this;
    }

    private_key private_key::regenerate( const fc::sha256& secret )
    {
       private_key self;
       self.my->_key = secret;
       return self;
    }

    fc::sha256 private_key::get_secret()const
    {
        return my->_key;
    }

    private_key::private_key( EC_KEY* k )
    {
       my->_key = get_secret( k );
       EC_KEY_free(k);
    }

    public_key private_key::get_public_key()const
    {
      FC_ASSERT(my->_key != empty_priv);
      public_key_data pub;
      size_t pk_len = pub.size();

      secp256k1_pubkey pubkey;
      FC_ASSERT(secp256k1_ec_pubkey_create(detail::_get_context(), &pubkey, (const unsigned char*)my->_key.data()));

      secp256k1_ec_pubkey_serialize(detail::_get_context(), (unsigned char*)&pub.data[0], &pk_len, &pubkey, SECP256K1_EC_COMPRESSED);

      return public_key(pub);
    }

    static int extended_nonce_function(unsigned char *nonce32, 
                                       const unsigned char *msg32, 
                                       const unsigned char *key32, 
                                       const unsigned char *algo16, 
                                       void *data, unsigned int counter) 
    {
      unsigned int* extra = (unsigned int*) data;
      (*extra)++;
      return secp256k1_nonce_function_default(nonce32, msg32, key32, algo16, nullptr, *extra);
    }

    compact_signature private_key::sign_compact( const fc::sha256& digest )const
    {
      FC_ASSERT( my->_key != empty_priv );
      compact_signature result;
      int recid;
      unsigned int counter = 0;
      do
      {
        secp256k1_ecdsa_recoverable_signature sig;
        FC_ASSERT(secp256k1_ecdsa_sign_recoverable(detail::_get_context(),
                                                   &sig, 
                                                   (unsigned char*)digest.data(),
                                                   (unsigned char*)my->_key.data(), 
                                                   extended_nonce_function,
                                                   &counter));
        FC_ASSERT(secp256k1_ecdsa_recoverable_signature_serialize_compact(detail::_get_context(), (unsigned char*)result.begin() + 1,
                                                                          &recid, &sig));
        FC_ASSERT(recid != -1);
      } 
      while (!public_key::is_canonical(result));
      result.begin()[0] = 27 + 4 + recid;
      return result;
    }

    compact_signature compact_DER_signature(const unsigned char* data, size_t dataLen, const public_key_data& signerPublicKeyData, const sha256& digest)
    {
      secp256k1_pubkey eccPublicKey;
      ilog("Received signerPublicKeyData: ${d}", ("d", signerPublicKeyData));

      const secp256k1_context* ctx = detail::_get_context();
      const unsigned char* keyData = (const unsigned char*)signerPublicKeyData.data;
      const size_t keyDataLen = signerPublicKeyData.size();

      ilog("Received keyDataLen: ${keyDataLen}", (keyDataLen));

      int res = secp256k1_ec_pubkey_parse(ctx, &eccPublicKey, keyData, keyDataLen);
      ilog("Received res: ${res}", (res));

      FC_ASSERT(res == 1);

      secp256k1_ecdsa_signature signature = {0};
      FC_ASSERT(secp256k1_ecdsa_signature_parse_der(detail::_get_context(), &signature, data, dataLen));

      std::vector<unsigned char> serializedSignature;
      serializedSignature.resize(64);

      FC_ASSERT(secp256k1_ecdsa_signature_serialize_compact(detail::_get_context(), serializedSignature.data(), &signature));

      compact_signature result;

      for (int recoveryId = 0; recoveryId <= 3; ++recoveryId)
      {
        secp256k1_ecdsa_recoverable_signature recoverable_sig;
        if (secp256k1_ecdsa_recoverable_signature_parse_compact(detail::_get_context(), &recoverable_sig, serializedSignature.data(), recoveryId))
        {
          secp256k1_pubkey recovered_key;
          if (secp256k1_ecdsa_recover(detail::_get_context(), &recovered_key, &recoverable_sig, (const unsigned char*)digest.data()))
          {
            if (secp256k1_ec_pubkey_cmp(detail::_get_context(), &recovered_key, &eccPublicKey) == 0)
            {
              int receivedRecoveryId = 0;
              FC_ASSERT(secp256k1_ecdsa_recoverable_signature_serialize_compact(detail::_get_context(), (unsigned char*)result.begin() + 1,
                &receivedRecoveryId, &recoverable_sig));

              FC_ASSERT(receivedRecoveryId== recoveryId);

              result.begin()[0] = 27 + 4 + recoveryId;

              return result;
            }
          }
        }
      }

      FC_ASSERT(false, "Unable to convert to compact_signature");

      return result;
    }

    void private_key_to_DER(const private_key& pk, FILE* stream)
    {
      const auto secret = pk.get_secret();

      const public_key pub_key = pk.get_public_key();
      const auto pub_key_data = pub_key.serialize_ecc_point();

      std::vector<unsigned char> der_pub_key_data;
      der_pub_key_data.reserve(pub_key_data.size() + 1);

      der_pub_key_data.push_back(POINT_CONVERSION_UNCOMPRESSED);
      der_pub_key_data.insert(der_pub_key_data.end(), pub_key_data.data, pub_key_data.data + pub_key_data.size());

      BIGNUM* pkSecret = BN_bin2bn((const unsigned char*)secret.data(), secret.data_size(), NULL);

      OSSL_PARAM* exampleParams = NULL;

      EVP_PKEY* examplekey = EVP_EC_gen(SN_secp256k1);

      EVP_PKEY_todata(examplekey, EVP_PKEY_KEYPAIR, &exampleParams);

      for (OSSL_PARAM* p = exampleParams; p->key != NULL; ++p)
      {
        //ilog("param: key: ${key}, data_type: ${data_type}, data_size: ${data_size}", ("key", p->key)("data_type", p->data_type)("data_size", p->data_size));

        if(strcmp(p->key, "priv") == 0)
          OSSL_PARAM_set_BN(p, pkSecret);
        else
        if(strcmp(p->key, "pub") == 0)
          OSSL_PARAM_set_octet_string(p, der_pub_key_data.data(), der_pub_key_data.size());
      }

      EVP_PKEY* eccPrivateKey = NULL;

      EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
      EVP_PKEY_fromdata_init(ctx);
      auto rc = EVP_PKEY_fromdata(ctx, &eccPrivateKey, EVP_PKEY_KEYPAIR, exampleParams);

      ilog("EVP_PKEY_fromdata rc: ${rc}", (rc));

      BIO* out = BIO_new_fp(stream, BIO_NOCLOSE);

      i2d_PrivateKey_bio(out, eccPrivateKey);

      BIO_free_all(out);

      EVP_PKEY_free(eccPrivateKey);
      EVP_PKEY_CTX_free(ctx);

      BN_free(pkSecret);
    }

}}
