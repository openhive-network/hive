#/bin/sh

BUILD_DIR="${PWD}/build"
TESTS_DIR=${BUILD_DIR}/tests

cd $TESTS_DIR

# $1 unit test name group
execute_unittest_group()
{
  local unit_test_group=$1
  echo "Start unit tests group '${unit_test_group}'"
  if ! ctest -R ^unit/${unit_test_group}.* --output-on-failure -vv
  then
    exit 1
  fi
}

# $1 ctest test name
execute_exactly_one_test()
{
  local ctest_test_name=$1
  echo "Start ctest test '${ctest_test_name}'"
  if ! ctest -R ^${ctest_test_name}$ --output-on-failure -vv
  then
    exit 1
  fi
}

execute_hive_functional()
{
  echo "Start hive functional tests"
  if ! ctest -R ^functional/.* --output-on-failure -vv
  then
    exit 1
  fi
}

echo "      _____                    _____                    _____                _____                    _____          ";
echo "     /\    \                  /\    \                  /\    \              /\    \                  /\    \         ";
echo "    /::\    \                /::\    \                /::\    \            /::\    \                /::\    \        ";
echo "    \:::\    \              /::::\    \              /::::\    \           \:::\    \              /::::\    \       ";
echo "     \:::\    \            /::::::\    \            /::::::\    \           \:::\    \            /::::::\    \      ";
echo "      \:::\    \          /:::/\:::\    \          /:::/\:::\    \           \:::\    \          /:::/\:::\    \     ";
echo "       \:::\    \        /:::/__\:::\    \        /:::/__\:::\    \           \:::\    \        /:::/__\:::\    \    ";
echo "       /::::\    \      /::::\   \:::\    \       \:::\   \:::\    \          /::::\    \       \:::\   \:::\    \   ";
echo "      /::::::\    \    /::::::\   \:::\    \    ___\:::\   \:::\    \        /::::::\    \    ___\:::\   \:::\    \  ";
echo "     /:::/\:::\    \  /:::/\:::\   \:::\    \  /\   \:::\   \:::\    \      /:::/\:::\    \  /\   \:::\   \:::\    \ ";
echo "    /:::/  \:::\____\/:::/__\:::\   \:::\____\/::\   \:::\   \:::\____\    /:::/  \:::\____\/::\   \:::\   \:::\____\ ";
echo "   /:::/    \::/    /\:::\   \:::\   \::/    /\:::\   \:::\   \::/    /   /:::/    \::/    /\:::\   \:::\   \::/    /";
echo "  /:::/    / \/____/  \:::\   \:::\   \/____/  \:::\   \:::\   \/____/   /:::/    / \/____/  \:::\   \:::\   \/____/ ";
echo " /:::/    /            \:::\   \:::\    \       \:::\   \:::\    \      /:::/    /            \:::\   \:::\    \     ";
echo "/:::/    /              \:::\   \:::\____\       \:::\   \:::\____\    /:::/    /              \:::\   \:::\____\    ";
echo "\::/    /                \:::\   \::/    /        \:::\  /:::/    /    \::/    /                \:::\  /:::/    /    ";
echo " \/____/                  \:::\   \/____/          \:::\/:::/    /      \/____/                  \:::\/:::/    /     ";
echo "                           \:::\    \               \::::::/    /                                 \::::::/    /      ";
echo "                            \:::\____\               \::::/    /                                   \::::/    /       ";
echo "                             \::/    /                \::/    /                                     \::/    /        ";
echo "                              \/____/                  \/____/                                       \/____/         ";
echo "                                                                                                                     ";




execute_exactly_one_test all_plugin_tests
execute_exactly_one_test all_chain_tests

execute_hive_functional

exit 0
