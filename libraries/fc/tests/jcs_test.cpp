#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <boost/test/unit_test.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/log/logger.hpp>

#include <fstream>

BOOST_AUTO_TEST_SUITE(fc)

// jcs test vectors from https://github.com/cyberphone/json-canonicalization
struct json_test_vector {
  std::string json_input;
  std::string correct_jcs_output;
};
std::vector<json_test_vector> json_test_vectors = {
  { // arrays.json
    u8R"([
  56,
  {
    "d": true,
    "10": null,
    "1": [ ]
  }
])",
    u8R"([56,{"1":[],"10":null,"d":true}])"
  },
  { // french.json
    u8R"({
  "peach": "This sorting order",
  "péché": "is wrong according to French",
  "pêche": "but canonicalization MUST",
  "sin":   "ignore locale"
})",
    u8R"({"peach":"This sorting order","péché":"is wrong according to French","pêche":"but canonicalization MUST","sin":"ignore locale"})"
  },
  { // structures.json
    u8R"({
  "1": {"f": {"f": "hi","F": 5} ,"\n": 56.0},
  "10": { },
  "": "empty",
  "a": { },
  "111": [ {"e": "yes","E": "no" } ],
  "A": { }
})",
    u8R"({"":"empty","1":{"\n":56,"f":{"F":5,"f":"hi"}},"10":{},"111":[{"E":"no","e":"yes"}],"A":{},"a":{}})"
  },
  { // unicode.json
    u8R"({
  "Unnormalized Unicode":"A\u030a"
})",
    u8R"({"Unnormalized Unicode":"Å"})"
  },
  { // values.json
    u8R"({
  "numbers": [333333333.33333329, 1E30, 4.50, 2e-3, 0.000000000000000000000000001],
  "string": "\u20ac$\u000F\u000aA'\u0042\u0022\u005c\\\"\/",
  "literals": [null, true, false]
})",
    u8R"({"literals":[null,true,false],"numbers":[333333333.3333333,1e+30,4.5,0.002,1e-27],"string":"€$\u000f\nA'B\"\\\\\"/"})"
  },
  { // weird.json
    u8R"({
  "\u20ac": "Euro Sign",
  "\r": "Carriage Return",
  "\u000a": "Newline",
  "1": "One",
  "\u0080": "Control\u007f",
  "\ud83d\ude02": "Smiley",
  "\u00f6": "Latin Small Letter O With Diaeresis",
  "\ufb33": "Hebrew Letter Dalet With Dagesh",
  "</script>": "Browser Challenge"
})",
    u8R"({"\n":"Newline","\r":"Carriage Return","1":"One","</script>":"Browser Challenge","":"Control","ö":"Latin Small Letter O With Diaeresis","€":"Euro Sign","😂":"Smiley","דּ":"Hebrew Letter Dalet With Dagesh"})"
  }
};


BOOST_AUTO_TEST_CASE(jcs_test)
{
  for (const json_test_vector& test_vector : json_test_vectors) {
    std::string generated_jcs = fc::json::json_to_jcs(test_vector.json_input);
    BOOST_CHECK_EQUAL(generated_jcs, test_vector.correct_jcs_output);
  }
}

// The tests above are the only "official" JCS test vectors, but they're hardly comprehensive.
// The rest of this file is testing the JCS number formatting code, which is the most complicated
// part and has a good test suite available.

// number test vectors from RFC 8785
struct number_test_vector {
  std::string double_hex_bytes;
  std::string correct_string_representation;
};
std::vector<number_test_vector> number_test_vectors = {
  {"0000000000000000","0"},
  {"8000000000000000","0"},
  {"0000000000000001","5e-324"},
  {"8000000000000001","-5e-324"},
  {"7fefffffffffffff","1.7976931348623157e+308"},
  {"ffefffffffffffff","-1.7976931348623157e+308"},
  {"4340000000000000","9007199254740992"},
  {"c340000000000000","-9007199254740992"},
  {"4430000000000000","295147905179352830000"},
  {"44b52d02c7e14af5","9.999999999999997e+22"},
  {"44b52d02c7e14af6","1e+23"},
  {"44b52d02c7e14af7","1.0000000000000001e+23"},
  {"444b1ae4d6e2ef4e","999999999999999700000"},
  {"444b1ae4d6e2ef4f","999999999999999900000"},
  {"444b1ae4d6e2ef50","1e+21"},
  {"3eb0c6f7a0b5ed8c","9.999999999999997e-7"},
  {"3eb0c6f7a0b5ed8d","0.000001"},
  {"41b3de4355555553","333333333.3333332"},
  {"41b3de4355555554","333333333.33333325"},
  {"41b3de4355555555","333333333.3333333"},
  {"41b3de4355555556","333333333.3333334"},
  {"41b3de4355555557","333333333.33333343"},
  {"becbf647612f3696","-0.0000033333333333333333"},
  {"43143ff3c1cb0959","1424953923781206.2"}
};


static double hex_string_to_double(const std::string& hex_string) {
  char binary_value[8];
  fc::from_hex(hex_string, (char*)&binary_value, 8);
  // test vectors are in big-endian
  std::reverse(binary_value, binary_value + 8);
  return *reinterpret_cast<double*>(binary_value);
}

// Run the test cases explicitly listed in RFC 8785
BOOST_AUTO_TEST_CASE(number_test)
{
  for (const number_test_vector& test_vector : number_test_vectors) {
    double double_value = hex_string_to_double(test_vector.double_hex_bytes);
    std::string double_to_jcs_result = fc::json::double_to_jcs(double_value);

    BOOST_CHECK_MESSAGE(double_to_jcs_result == test_vector.correct_string_representation,
                        "invalid result for double with hex value of " << test_vector.double_hex_bytes << ", expected " << test_vector.correct_string_representation << " but got " << double_to_jcs_result);
  }
  BOOST_CHECK_THROW(fc::json::double_to_jcs(hex_string_to_double("7fffffffffffffff")), fc::invalid_arg_exception); // NaN
  BOOST_CHECK_THROW(fc::json::double_to_jcs(hex_string_to_double("7ff0000000000000")), fc::invalid_arg_exception); // Infinity
}

// This test reads the a test file with 100M cases.  The file is available here:
// https://github.com/cyberphone/json-canonicalization/releases/download/es6testfile/es6testfile100m.txt.gz
// note: this is testing the underlying double_to_es6 function instead of double_to_jcs, because the 
// test vectors assume Infinity and NaN are valid
BOOST_AUTO_TEST_CASE(number_test_from_file)
{
  std::ifstream test_vectors("es6testfile100m.txt");
  if (!test_vectors) {
    BOOST_TEST_MESSAGE("Skipping number_test_from_file because test vector file is not present.");
    BOOST_TEST_MESSAGE("To run this test, download the vectors from:");
    BOOST_TEST_MESSAGE("    https://github.com/cyberphone/json-canonicalization/releases/download/es6testfile/es6testfile100m.txt.gz");
    BOOST_TEST_MESSAGE("and re-run this test");
    return;
  }
  for (std::string line; std::getline(test_vectors, line); ) {
    size_t comma_pos = line.find(',');
    if (comma_pos != std::string::npos) {
      std::string hex_value = line.substr(0, comma_pos);
      std::string correct_string_representation = line.substr(comma_pos + 1);

      // in the test file, leading zeros are omitted.  fix that
      hex_value.insert(0, 16 - hex_value.length(), '0');
      double double_value = hex_string_to_double(hex_value);

      std::string double_to_es6_result = fc::json::double_to_es6(double_value);

      BOOST_CHECK_MESSAGE(double_to_es6_result == correct_string_representation, "invalid result for double with hex value of " << hex_value << ", expected " << correct_string_representation << " but got " << double_to_es6_result);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
