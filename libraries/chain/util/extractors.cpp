#include <hive/chain/util/extractors.hpp>

using hive::app::Inserters::Member;

std::ostream &hive::app::Inserters::operator<<(std::ostream &os, const Member &m)
{
  switch (m)
  {
  case Member::ACCOUNT_CREATE_OPERATION:
  {
    os << "ACCOUNT_CREATE_OPERATION";
    break;
  }
  case Member::FEE:
  {
    os << "FEE";
    break;
  }
  case Member::DELEGATION:
  {
    os << "DELEGATION";
    break;
  }
  case Member::MAX_ACCEPTED_PAYOUT:
  {
    os << "MAX_ACCEPTED_PAYOUT";
    break;
  }
  case Member::AMOUNT:
  {
    os << "AMOUNT";
    break;
  }
  case Member::HBD_AMOUNT:
  {
    os << "HBD_AMOUNT";
    break;
  }
  case Member::HIVE_AMOUNT:
  {
    os << "HIVE_AMOUNT";
    break;
  }
  case Member::VESTING_SHARES:
  {
    os << "VESTING_SHARES";
    break;
  }
  case Member::AMOUNT_TO_SELL:
  {
    os << "AMOUNT_TO_SELL";
    break;
  }
  case Member::MIN_TO_RECEIVE:
  {
    os << "MIN_TO_RECEIVE";
    break;
  }
  case Member::REWARD_HIVE:
  {
    os << "REWARD_HIVE";
    break;
  }
  case Member::REWARD_HBD:
  {
    os << "REWARD_HBD";
    break;
  }
  case Member::REWARD_VESTS:
  {
    os << "REWARD_VESTS";
    break;
  }
  case Member::SMT_CREATION_FEE:
  {
    os << "SMT_CREATION_FEE";
    break;
  }
  case Member::LEP_ABS_AMOUNT:
  {
    os << "LEP_ABS_AMOUNT";
    break;
  }
  case Member::REP_ABS_AMOUNT:
  {
    os << "REP_ABS_AMOUNT";
    break;
  }
  case Member::CONTRIBUTION:
  {
    os << "CONTRIBUTION";
    break;
  }
  case Member::DAILY_PAY:
  {
    os << "DAILY_PAY";
    break;
  }
  case Member::PAYMENT:
  {
    os << "PAYMENT";
    break;
  }
  default:
  {
    os << "UNKNOWN";
    break;
  }
  };
  return os;
}