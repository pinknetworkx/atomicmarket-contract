#include <atomicmarket.hpp>

#include "admin_actions.cpp"
#include "sale_actions.cpp"
#include "stablesale_actions.cpp"
#include "auction_actions.cpp"
#include "log_actions.cpp"
#include "notification_handling.cpp"
#include "internal_functions.cpp"
#include "util_functions.cpp"

//The functionality is split in multiple files for better readability.



/**
* Withdraws a token from a users balance. The specified quantity is then transferred to the user.
*/
ACTION atomicmarket::withdraw(
  name from,
  asset quantity
) {
  require_auth(from);

  config_s current_config = config.get();

  bool found_token = false;
  for (TOKEN supported_token : current_config.supported_tokens) {
    if (supported_token.token_symbol == quantity.symbol) {
      found_token = true;

      internal_deduct_balance(from, quantity, string("Withdrawal"));

      action(
        permission_level{get_self(), name("active")},
        supported_token.token_contract,
        name("transfer"),
        make_tuple(
          get_self(),
          from,
          quantity,
          string("Withdrawal")
        )
      ).send();
    }
  }

  check(found_token, "The specified asset symbol is not supported");
}