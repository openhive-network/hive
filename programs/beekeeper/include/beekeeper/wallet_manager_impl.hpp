#pragma once

#include <beekeeper/beekeeper_wallet_base.hpp>
#include <beekeeper/utilities.hpp>
#include <beekeeper/singleton_beekeeper.hpp>

namespace fc { class variant; }

namespace beekeeper {

using collector_t = hive::utilities::notifications::collector_t;

class wallet_manager_impl {
  public:

    using wallet_filename_creator_type = std::function<boost::filesystem::path(const std::string&)>;

    wallet_manager_impl(){}

    std::string create( wallet_filename_creator_type wallet_filename_creator, const std::string& name, fc::optional<std::string> password = fc::optional<std::string>{} );
    void open( wallet_filename_creator_type wallet_filename_creator, const std::string& name );
    std::vector<wallet_details> list_wallets();
    map<std::string, std::string> list_keys( const string& name, const string& pw );
    flat_set<std::string> get_public_keys();
    void lock_all();
    void lock( const std::string& name );
    void unlock( wallet_filename_creator_type wallet_filename_creator, const std::string& name, const std::string& password );
    string import_key( const std::string& name, const std::string& wif_key );
    void remove_key( const std::string& name, const std::string& password, const std::string& key );
    string create_key( const std::string& name );
    signature_type sign_digest( const digest_type& digest, const public_key_type& key );

  private:

    std::map<std::string, std::unique_ptr<beekeeper_wallet_base>> wallets;

    std::string gen_password();
    bool valid_filename( const string& name );
};

} //beekeeper

