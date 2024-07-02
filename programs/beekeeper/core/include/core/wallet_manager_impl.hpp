#pragma once

#include <core/beekeeper_wallet_base.hpp>
#include <core/utilities.hpp>

#include <boost/filesystem.hpp>

namespace fc { class variant; }

namespace beekeeper {

class wallet_manager_impl {
  private:

    std::vector<wallet_details> list_wallets_impl( const std::vector< std::string >& wallet_files );
    std::vector< std::string > list_created_wallets_impl( const boost::filesystem::path& directory, const std::string& extension ) const;

    fc::optional<private_key_type> find_private_key_in_given_wallet( const public_key_type& public_key, const string& wallet_name );
    keys_details list_keys_impl( const string& name, const string& pw, bool password_is_required );

  public:

    wallet_manager_impl( const boost::filesystem::path& _wallet_directory ): wallet_directory( _wallet_directory ){}

    std::string create( const std::string& wallet_name, const std::optional<std::string>& password );
    void open( const std::string& wallet_name );
    void close( const std::string& wallet_name );
    std::vector<wallet_details> list_wallets();
    std::vector<wallet_details> list_created_wallets();
    keys_details list_keys( const string& name, const string& password );
    keys_details get_public_keys( const std::optional<std::string>& wallet_name );
    void lock_all();
    void lock( const std::string& wallet_name );
    void unlock( const std::string& wallet_name, const std::string& password );
    string import_key( const std::string& name, const std::string& wif_key, const std::string& prefix );
    std::vector<string> import_keys( const std::string& name, const std::vector<string>& wif_keys, const std::string& prefix );
    void remove_key( const std::string& name, const public_key_type& public_key );
    signature_type sign_digest( const std::optional<std::string>& wallet_name, const digest_type& sig_digest, const public_key_type& public_key, const std::string& prefix );
    bool has_matching_private_key( const std::string& wallet_name, const public_key_type& public_key );
    std::string encrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& content, const std::optional<unsigned int>& nonce, const std::string& prefix );
    std::string decrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& encrypted_content );

  private:

    const uint32_t max_password_length  = 128;
    const std::string file_ext          = ".wallet";

    boost::filesystem::path wallet_directory;

    std::map<std::string, std::unique_ptr<beekeeper_wallet_base>> wallets;

    std::string gen_password();
    void valid_filename( const string& name );

    bool is_locked( const string& name );

    signature_type sign( std::function<std::optional<signature_type>(const std::unique_ptr<beekeeper_wallet_base>&)>&& sign_method, const std::optional<std::string>& wallet_name, const public_key_type& public_key, const std::string& prefix );

    boost::filesystem::path create_wallet_filename( const std::string& wallet_name ) const
    {
      return wallet_directory / ( wallet_name + file_ext );
    }

    const std::string& get_extension() const
    {
      return file_ext;
    }

    const boost::filesystem::path& get_wallet_directory() const
    {
      return wallet_directory;
    }
};

} //beekeeper

