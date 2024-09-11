#include <signal.h>

#include <iostream>
#include <cstring>

namespace fc
{
  void sigbus_handler( int sig_num )
  {
    std::cerr << "Problem with memory allocation\n" << std::endl;

    std::exit(EXIT_FAILURE);
  }

  void print_information_on_sigbus()
  {
    struct sigaction sigact;

    memset( &sigact, 0, sizeof( sigact ) );

    sigact.sa_handler = sigbus_handler;
    sigact.sa_flags = 0;

    if( sigaction(SIGBUS, &sigact, (struct sigaction *)NULL) != 0 )
    {
        std::cerr << "Error setting signal handler for SIGBUS" << std::endl;
        std::exit(EXIT_FAILURE);
    }
  }

}