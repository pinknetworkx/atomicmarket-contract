#include <atomicmarket.hpp>

/**
* Gets the assets table with the scope owner from the atomicassets contract
*/
atomicmarket::atomicassets_assets_t atomicmarket::get_atomicassets_assets(name acc) {
  return atomicassets_assets_t(ATOMICASSETS_ACCOUNT, acc.value);
}


/**
* Gets the sales table with the scope seller
*/
atomicmarket::sales_t atomicmarket::get_sales(name acc) {
  return sales_t(get_self(), acc.value);
}


/**
* Gets the sales table with the scope seller
*/
atomicmarket::stablesales_t atomicmarket::get_stablesales(name acc) {
  return stablesales_t(get_self(), acc.value);
}


/**
* Gets the auctions table with the scope seller
*/
atomicmarket::auctions_t atomicmarket::get_auctions(name acc) {
  return auctions_t(get_self(), acc.value);
}


/**
* Gets the delphioracle datapoints table with the scope pair_name
*/
atomicmarket::datapoints_t atomicmarket::get_delphioracle_datapoints(name pair_name) {
  return datapoints_t(DELPHIORACLE_ACCOUNT, pair_name.value);
}




/**
* Checks if the provided marketplace is a valid marketplace
* A marketplace is valid either if it is empty ("") or if is in the marketplaces table
*/
bool atomicmarket::is_valid_marketplace(name marketplace) {
  return (marketplace == name("") || marketplaces.find(marketplace.value) != marketplaces.end());
}




/**
* Gets the author of a collection in the atomicassets contract
*/
name atomicmarket::get_collection_author(uint64_t asset_id, name asset_owner) {
  atomicassets_assets_t owner_assets = get_atomicassets_assets(asset_owner);
  auto asset_itr = owner_assets.find(asset_id);

  auto collection_itr = atomicassets_collections.find(asset_itr->collection_name.value);
  return collection_itr->author;
}


/**
* Gets the fee defined by a collection in the atomicassets contract
*/
double atomicmarket::get_collection_fee(uint64_t asset_id, name asset_owner) {
  atomicassets_assets_t owner_assets = get_atomicassets_assets(asset_owner);
  auto asset_itr = owner_assets.find(asset_id);

  auto collection_itr = atomicassets_collections.find(asset_itr->collection_name.value);
  return collection_itr->market_fee;
}




/**
* Internal function to check whether an token is a supported token
*/
bool atomicmarket::is_token_supported(
  name token_contract,
  symbol token_symbol
) {
  config_s current_config = config.get();

  for (TOKEN supported_token : current_config.supported_tokens) {
    if (supported_token.token_contract == token_contract && supported_token.token_symbol == token_symbol) {
      return true;
    }
  }
  return false;
}


/**
* Internal function to check whether a supported token with this symbol exists
*/
bool atomicmarket::is_symbol_supported(
  symbol token_symbol
) {
  config_s current_config = config.get();

  for (TOKEN supported_token : current_config.supported_tokens) {
    if (supported_token.token_symbol == token_symbol) {
      return true;
    }
  }
  return false;
}