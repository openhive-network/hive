#pragma once

#include <beekeeper/beekeeper_wallet_base.hpp>
#include <beekeeper/utilities.hpp>
#include <beekeeper/singleton_beekeeper.hpp>

namespace fc { class variant; }

namespace beekeeper {

using collector_t = hive::utilities::notifications::collector_t;

class wallet_manager_impl {
  public:

    wallet_manager_impl( const boost::filesystem::path& command_line_wallet_dir );

    std::string create( const std::string& token, const std::string& name, fc::optional<std::string> password = fc::optional<std::string>{} );
    void open( const std::string& token, const std::string& name );
    std::vector<wallet_details> list_wallets( const std::string& token );
    map<std::string, std::string> list_keys( const std::string& token, const string& name, const string& pw );
    flat_set<std::string> get_public_keys( const std::string& token );
    void lock_all( const std::string& token );
    void lock( const std::string& token, const std::string& name );
    void unlock( const std::string& token, const std::string& name, const std::string& password );
    string import_key( const std::string& token, const std::string& name, const std::string& wif_key );
    void remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& key );
    string create_key( const std::string& token, const std::string& name );
    signature_type sign_digest( const std::string& token, const digest_type& digest, const public_key_type& key );

    bool start();
    void save_connection_details( const collector_t& values );

  private:

    std::map<std::string, std::unique_ptr<beekeeper_wallet_base>> wallets;

    std::unique_ptr<singleton_beekeeper> singleton;

    std::string gen_password();
    bool valid_filename( const string& name );
};

} //beekeeper

