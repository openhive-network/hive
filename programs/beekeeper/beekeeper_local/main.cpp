#include <beekeeper_local/beekeeper_local_app.hpp>

#include<memory>

int main( int argc, char** argv )
{
  return beekeeper::beekeeper_app_base::execute( std::make_unique<beekeeper::beekeeper_local_app>(), argc, argv );
}
