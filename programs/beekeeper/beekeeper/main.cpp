#include <beekeeper/beekeeper_app.hpp>

#include<memory>

int main( int argc, char** argv )
{
  return beekeeper::beekeeper_app_base::execute( std::make_unique<beekeeper::beekeeper_app>(), argc, argv );
}
