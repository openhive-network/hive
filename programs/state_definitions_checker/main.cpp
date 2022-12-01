#include <hive/chain/account_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/transaction_object.hpp>

#include <iostream>

class object_member_visitor
{
  public:
    object_member_visitor(size_t& _checksum) : checksum(_checksum) {}

    template<typename Member, class Class, Member (Class::*member)>
    void operator()( const char* name )const
    {
      checksum += pos + sizeof(Member);
      ++pos;
      std::cout << "    " << name <<": " << fc::get_typename<Member>::name() << ", type size: " << sizeof(Member) << "\n";
    }
  private:
    size_t& checksum;
    mutable size_t pos = 0; 
};

template<typename T>
void GetTypeMembers()
{
  std::cout << "Type: " << fc::get_typename<T>::name() << "\n";
  std::cout << "It's members: \n";
  size_t checksum = 0;
  fc::reflector<T>::visit( object_member_visitor(checksum) );
  std::cout << "Final checksum: " << checksum << "\n";
}

int main( int argc, char** argv )
{
  try
  {
    GetTypeMembers<hive::chain::account_object>();
    GetTypeMembers<hive::chain::witness_object>();
    GetTypeMembers<hive::chain::transaction_object>();
  }
  catch ( const fc::exception& e )
  {
    edump((e.to_detail_string()));
  }

  return 0;
}
