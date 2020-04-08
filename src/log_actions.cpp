#include <atomicmarket.hpp>

ACTION atomicmarket::logincbal(
  name user,
  asset balance_increase,
  string reason
) {
  require_auth(get_self());
};

ACTION atomicmarket::logdecbal(
  name user,
  asset balance_decrease,
  string reason
) {
  require_auth(get_self());
};