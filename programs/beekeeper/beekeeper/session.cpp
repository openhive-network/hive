#include <beekeeper/session.hpp>

#include <core/beekeeper_instance_base.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

session::session( wallet_content_handlers_deliverer& content_deliverer, const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory )
        : session_base( content_deliverer, token, time, wallet_directory )
{
}

} //beekeeper
