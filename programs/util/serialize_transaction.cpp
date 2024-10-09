#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>

#include <fc/io/json.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/variant.hpp>

#include <hive/protocol/transaction.hpp>
#include <hive/protocol/types.hpp>

int main(int argc, char** argv, char** envp)
{
  const char* inputFileName = nullptr;

  if(argc == 2)
  {
    inputFileName = argv[1];
  }
  else
  if(argc > 2)
  {
    std::cerr << "Usage: serialize_transaction [input_tx.json]" << std::endl;
    std::cerr << "       If file name is omitted, trasaction can be passed as single line to stdin. Otherwise, additionaly binary form will be dumped to the <input_tx.json>.bin file" << std::endl;
    return 0;
  }

  std::string txJson;

  if (inputFileName != nullptr)
  {
    std::ifstream t(inputFileName);
    std::stringstream buffer;
    buffer << t.rdbuf();
    txJson = buffer.str();
  }
  else
  {
    while (std::cin)
    {
      std::getline(std::cin, txJson);
      boost::trim(txJson);
      if (txJson.empty() == false)
        break;
    }
  }

  try
    {
      fc::variant v = fc::json::from_string(txJson, fc::json::format_validation_mode::full, fc::json::strict_parser );
      hive::protocol::signed_transaction tx;

      hive::protocol::serialization_mode_controller::mode_guard guard(hive::protocol::transaction_serialization_type::hf26);
      hive::protocol::serialization_mode_controller::set_pack(hive::protocol::transaction_serialization_type::hf26);

      fc::from_variant(v, tx);

      const auto binaryStorage = fc::raw::pack_to_vector(tx);
      const auto hexForm = fc::to_hex(binaryStorage);

      if(inputFileName != nullptr)
      {
        std::string binFile(inputFileName);
        binFile += ".bin";
        std::ofstream outStream;
        outStream.open(binFile, std::ofstream::binary|std::ofstream::trunc);
        FC_ASSERT(outStream, "Cannot open output file for writing: ${binFile}", (binFile));
        FC_ASSERT(outStream.write(binaryStorage.data(), binaryStorage.size()), "Cannot write to the output file: ${binFile}", (binFile));
        outStream.close();
      }

      std::cout << hexForm << std::endl;

      return 0;
    }
    catch( const fc::exception& e )
    {
      std::cerr << e.to_detail_string() << std::endl;
      return 2;
    }
  }
