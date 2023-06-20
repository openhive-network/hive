#include <beekeeper_wasm/beekeeper_wasm_app.hpp>

#include<memory>

int main( int argc, char** argv )
{
  beekeeper::beekeeper_wasm_app _app;
  return _app.execute( argc, argv );
}
