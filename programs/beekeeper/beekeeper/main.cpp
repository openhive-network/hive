#include <beekeeper/beekeeper_app.hpp>

#include<memory>

int main( int argc, char** argv )
{
  beekeeper::beekeeper_app _app;
  return _app.init( argc, argv );
}
