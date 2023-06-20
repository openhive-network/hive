#include <beekeeper_local/beekeeper_local_app.hpp>

#include<memory>

int main( int argc, char** argv )
{
  beekeeper::beekeeper_local_app _app;
  return _app.execute( argc, argv );
}
