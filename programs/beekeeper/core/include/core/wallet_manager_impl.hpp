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

  public:

    wallet_manager_impl( const boost::filesystem::path& _wallet_directory ): wallet_directory( _wallet_directory ){}

    std::string create( const std::string& name, const std::optional<std::string>& password );
    void open( const std::string& name );
    void close( const std::string& name );
    std::vector<wallet_details> list_wallets();
    std::vector<wallet_details> list_created_wallets();
    map<public_key_type, private_key_type> list_keys( const string& name, const string& pw );
    flat_set<public_key_type> get_public_keys( const std::optional<std::string>& wallet_name );
    void lock_all();
    void lock( const std::string& name );
    void unlock( const std::string& name, const std::string& password );
    string import_key( const std::string& name, const std::string& wif_key );
    void remove_key( const std::string& name, const std::string& password, const std::string& public_key );
    signature_type sign_digest( const digest_type& sig_digest, const public_key_type& public_key, const std::optional<std::string>& wallet_name );
    bool has_matching_private_key( const std::string& name, const public_key_type& public_key );

  private:

    const uint32_t max_password_length  = 128;
    const std::string file_ext          = ".wallet";

    boost::filesystem::path wallet_directory;

    std::map<std::string, std::unique_ptr<beekeeper_wallet_base>> wallets;

    std::string gen_password();
    void valid_filename( const string& name );

    bool is_locked( const string& name );

    signature_type sign( std::function<std::optional<signature_type>(const std::unique_ptr<beekeeper_wallet_base>&)>&& sign_method, const public_key_type& public_key, const std::optional<std::string>& wallet_name );

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

