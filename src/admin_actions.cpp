#include <atomicmarket.hpp>

/**
*  Initializes the config table. Only needs to be called once when first deploying the contract
* 
*  @required_auth The contract itself
*/
ACTION atomicmarket::init() {
  require_auth(get_self());
  config.get_or_create(get_self(),config_s{});

  marketplaces.emplace(get_self(), [&](auto& _marketplace) {
    _marketplace.marketplace_name = DEFAULT_MARKETPLACE_NAME;
    _marketplace.fee_account = DEFAULT_MARKETPLACE_FEE;
  });
}


/**
*  Sets the minimum bid increase compared to the previous bid
* 
*  @required_auth The contract itself
*/
ACTION atomicmarket::setminbidinc(double minimum_bid_increase) {
  require_auth(get_self());
  config_s current_config = config.get();

  current_config.minimum_bid_increase = minimum_bid_increase;

  config.set(current_config, get_self());
}


/**
*  Adds a token that can be used to sell assets for
* 
*  @required_auth The contract itself
*/
ACTION atomicmarket::addconftoken(name token_contract, symbol token_symbol) {
  require_auth(get_self());

  check(!is_symbol_supported(token_symbol),
  "A token with this symbol is already supported");

  config_s current_config = config.get();

  current_config.supported_tokens.push_back({
    .token_contract = token_contract,
    .token_symbol = token_symbol
  });

  config.set(current_config, get_self());
}


/**
* Sets the maker and taker market fee
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::setmarketfee(double maker_market_fee, double taker_market_fee) {
  require_auth(get_self());
  config_s current_config = config.get();

  current_config.maker_market_fee = maker_market_fee;
  current_config.taker_market_fee = taker_market_fee;

  config.set(current_config, get_self());
}




/**
* Registers a marketplace that can then be used in the maker_marketplace / taker_marketplace parameters
* 
* This is needed because without the registration process, an attacker could create tiny sales with random accounts
* as the marketplace, for which the atomicmarket contract would then create balance table rows and pay the RAM for.
* 
* marketplace names that belong to existing accounts can not be chosen,
* except if that account authorizes the transaction
* 
* @required_auth fee_account
*/
ACTION atomicmarket::regmarket(
  name fee_account,
  name marketplace_name
) {
  require_auth(fee_account);

  check(!is_account(marketplace_name) || has_auth(marketplace_name),
  "Can't create a marketplace with a name of an existing account without its authorization");

  check(marketplaces.find(marketplace_name.value) == marketplaces.end(),
  "A marketplace with this name already exists");

  marketplaces.emplace(fee_account, [&](auto& _marketplace) {
    _marketplace.marketplace_name = marketplace_name;
    _marketplace.fee_account = fee_account;
  });
}