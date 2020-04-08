#include <atomicmarket.hpp>

/**
* Gives the seller, the marketplaces and the collection their share of the sale price
*/
void atomicmarket::internal_payout_sale(
  asset quantity,
  name seller,
  name maker_marketplace,
  name taker_marketplace,
  name collection_author,
  double collection_fee
) {
  config_s current_config = config.get();

  //Calculate cuts
  uint64_t maker_cut_amount = (uint64_t) (current_config.maker_market_fee * (double) quantity.amount);
  uint64_t taker_cut_amount = (uint64_t) (current_config.taker_market_fee * (double) quantity.amount);
  uint64_t collection_cut_amount = (uint64_t) (collection_fee * (double) quantity.amount);
  uint64_t seller_cut_amount = quantity.amount - maker_cut_amount - taker_cut_amount - collection_cut_amount;

  //Payout maker market
  auto maker_itr = marketplaces.find(maker_marketplace.value);
  internal_add_balance(
    maker_itr->fee_account,
    asset(maker_cut_amount, quantity.symbol),
    string("Maker Marketplace Fee")
  );

  //Payout taker market
  auto taker_itr = marketplaces.find(taker_marketplace.value);
  internal_add_balance(
    taker_itr->fee_account,
    asset(taker_cut_amount, quantity.symbol),
    string("Taker Marketplace Fee")
  );

  //Payout collection
  internal_add_balance(
    collection_author,
    asset(collection_cut_amount, quantity.symbol),
    string("Collection Market Fee")
  );

  //Payout seller
  internal_add_balance(
    seller,
    asset(seller_cut_amount, quantity.symbol),
    string("Asset Sale")
  );
}




/**
* Internal function used to add a quantity of a token to an account's balance
* It is not checked whether the added token is a supported token, this has to be checked before calling this function
*/
void atomicmarket::internal_add_balance(
  name user,
  asset quantity,
  string reason
) {
  if (quantity.amount == 0) {
    return;
  }

  auto balance_itr = balances.find(user.value);

  vector<asset> quantities;
  if (balance_itr == balances.end()) {
    //No balance table row exists yet
    quantities = {quantity};
    balances.emplace(get_self(), [&](auto& _balance) {
      _balance.owner = user;
      _balance.quantities = quantities;
    });

  } else {
    //A balance table row already exists for user
    quantities = balance_itr->quantities;

    bool found_token = false;
    for (asset& token : quantities) {
      if (token.symbol == quantity.symbol) {
        //If the user already has a balance for the token, this balance is increased
        found_token = true;
        token.amount += quantity.amount;
        break;
      }
    }

    if (!found_token) {
      //If the user does not already have a balance for the token, it is added to the vector
      quantities.push_back(quantity);
    }

    balances.modify(balance_itr, get_self(), [&](auto& _balance) {
      _balance.quantities = quantities;
    });
  }

  action(
    permission_level{get_self(), name("active")},
    get_self(),
    name("logincbal"),
    make_tuple(
      user,
      quantity,
      reason
    )
  ).send();
}




/**
* Internal function used to deduct a quantity of a token from an account's balance
* If the account does not has less than that quantity in his balance, this function will cause the
* transaction to fail
*/
void atomicmarket::internal_deduct_balance(
  name user,
  asset quantity,
  string reason
) {
  auto balance_itr = balances.require_find(user.value,
  "The user does not have a sufficient balance (No balances row)");

  vector<asset> user_balance = balance_itr->quantities;

  bool found_token = false;
  for (auto itr = user_balance.begin(); itr != user_balance.end(); itr++) {
    if (itr->symbol == quantity.symbol) {
      found_token = true;

      check(itr->amount >= quantity.amount,
      "The user does not have a sufficient balance (Specific token balance exists but is too low)");

      itr->amount -= quantity.amount;
      if (itr->amount == 0) {
        user_balance.erase(itr);
      }

      break;
    }
  }

  check(found_token,
  "The user does not have a sufficient balance (Balances row exists, but no entry for specific token)");

  if (user_balance.size() > 0) {
    balances.modify(balance_itr, get_self(), [&](auto& _balance) {
      _balance.quantities = user_balance;
    });
  } else {
    balances.erase(balance_itr);
  }

  action(
    permission_level{get_self(), name("active")},
    get_self(),
    name("logdecbal"),
    make_tuple(
      user,
      quantity,
      reason
    )
  ).send();
}




void atomicmarket::internal_transfer_asset(
  name to,
  uint64_t asset_id,
  string memo
) {
  vector<uint64_t> assets = {asset_id};

  action(
    permission_level{get_self(), name("active")},
    ATOMICASSETS_ACCOUNT,
    name("transfer"),
    make_tuple(
      get_self(),
      to,
      assets,
      memo
    )
  ).send();
}