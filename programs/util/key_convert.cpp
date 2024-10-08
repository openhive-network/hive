#include <algorithm>
#include <iostream>
#include <string>
#include <cstddef>
#include <cstdio>

#include <boost/algorithm/string.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>

#include <fc/crypto/elliptic.hpp>


namespace fc { namespace ecc { void private_key_to_DER(const private_key& privKey, FILE* stream); } }

int main(int argc, char** argv, char** envp)
{
  try
  {
    while (std::cin)
    {
      std::string line;
      std::getline(std::cin, line);

      boost::trim(line);

      if(line.empty())
        continue;

      if (line.find(HIVE_ADDRESS_PREFIX) != std::string::npos)
      {
        /// STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4
        fc::ecc::public_key pubKey = fc::ecc::public_key::from_base58_with_prefix(line, HIVE_ADDRESS_PREFIX);
        const auto derData = pubKey.serialize();
        std::cout.write(derData.data, derData.size());
        std::cout.flush();
        return 0;
      }
      else
      {
        /// 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
        const auto convertionResult = fc::ecc::private_key::wif_to_key(line);
        FC_ASSERT(convertionResult, "Invalid WIF input: ${line}", (line));
        fc::ecc::private_key privKey = *convertionResult;

        fc::ecc::private_key_to_DER(privKey, stdout);
      }
    }

      return 0;
  }
  catch (fc::exception const& e)
  {
    std::cerr << e.to_detail_string() << '\n';
    return 2;
  }

  catch (std::exception const& e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
