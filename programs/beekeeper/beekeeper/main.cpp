#include <beekeeper/beekeeper_app.hpp>
#include <boost/scope_exit.hpp>

#include<memory>

int main( int argc, char** argv )
{
  BOOST_SCOPE_EXIT(void)
  {
    appbase::reset();
  } BOOST_SCOPE_EXIT_END

  beekeeper::beekeeper_app _app;
  return _app.init( argc, argv ).status ? 0 : 1;
}
