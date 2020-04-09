#include <atomicmarket.hpp>
#include <math.h>

/**
* Create a stable sale listing
* For the sale to become active, the seller needs to create an atomicassets offer from them to the atomicmarket
* account, offering (only) the asset to be sold with the memo "stablesale"
* 
* @required_auth seller
*/
ACTION atomicmarket::announcestbl(
  name seller,
  uint64_t asset_id,
  name delphi_pair_name,
  asset price,
  name maker_marketplace
) {
  require_auth(seller);

  atomicassets_assets_t seller_assets = get_atomicassets_assets(seller);
  seller_assets.require_find(asset_id,
  "You do not own the specified asset. You can only announce sales for assets that you currently own.");

  stablesales_t seller_stablesales = get_stablesales(seller);
  check(seller_stablesales.find(asset_id) == seller_stablesales.end(),
  "You have already announced a stable sale for this asset. You can cancel a stable sale using the cancelstbl action.");

  config_s current_config = config.get();
  bool found_delphi_pair = false;
  for (DELPHIPAIR delphi_pair : current_config.supported_delphi_pairs) {
    if (delphi_pair.delphi_pair_name == delphi_pair_name) {
      found_delphi_pair = true;

      check(price.symbol == delphi_pair.stable_symbol,
      "The specified price uses a different symbol than the one required by the specified deplhi pair name");
    }
  }
  check(found_delphi_pair, "The specified delphi pair name is not supported");

  check(price.amount >= 0, "The sale price must be positive");

  check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

  seller_stablesales.emplace(seller, [&](auto& _sale) {
    _sale.asset_id = asset_id;
    _sale.delphi_pair_name = delphi_pair_name;
    _sale.price = price;
    _sale.offer_id = -1;
    _sale.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
    _sale.collection_fee = get_collection_fee(asset_id, seller);
  });
}




/**
* Cancels a stable sale. The stable sale can both be active or inactive
* 
* @required_auth seller
*/
ACTION atomicmarket::cancelstbl(
  name seller,
  uint64_t asset_id
) {
  require_auth(seller);

  stablesales_t seller_stablesales = get_stablesales(seller);
  auto stablesale_itr = seller_stablesales.require_find(asset_id,
  "The seller does not have a stable sale with this asset_id");

  if (stablesale_itr->offer_id != -1) {
    if (atomicassets_offers.find(stablesale_itr->offer_id) != atomicassets_offers.end()) {
      //Cancels the atomicassets offer for this sale for convenience
      action(
        permission_level{get_self(), name("active")},
        ATOMICASSETS_ACCOUNT,
        name("declineoffer"),
        make_tuple(
          stablesale_itr->offer_id
        )
      ).send();
    }
  }

  seller_stablesales.erase(stablesale_itr);
}




/**
* Purchases an asset that is for sale (stable sale).
* The sale price is deducted from the buyer's balance and added to the seller's balance
* 
* The intended_price parameter must match the sale's price. This is to prevent possible attacks where a seller could
* try to increase the price of the sale very shortly before the buyer sends the buy auction
* 
* At least one datapoint must exist in the delphi oracle contract that has a median
* equal to the intended_delphi_median parameter. 
* This (opposed to always using the newest datapoint) is done so that is possible to predict exactly how much the
* purchase will cost when signing the action.
* 
* @required_auth buyer
*/
ACTION atomicmarket::purchasestbl(
  name buyer,
  name seller,
  uint64_t asset_id,
  asset intended_price,
  uint64_t intended_delphi_median,
  name taker_marketplace
) {
  require_auth(buyer);

  check(buyer != seller, "You can't purchase your own stable sale");

  stablesales_t seller_stablesales = get_stablesales(seller);
  auto stablesale_itr = seller_stablesales.require_find(asset_id,
  "The specified seller does not have a stable sale for this asset");

  check(stablesale_itr->offer_id != -1,
  "This sale is not active yet. The seller first has to create an atomicasset offer for this asset");

  check(atomicassets_offers.find(stablesale_itr->offer_id) != atomicassets_offers.end(),
  "The seller cancelled the atomicassets offer related to this stable sale");

  check(stablesale_itr->price.symbol == intended_price.symbol,
  "The intended price uses a different symbol than the stable sale price");

  check(stablesale_itr->price == intended_price, "The intended price does not match the stable sale's price");

  check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

  datapoints_t datapoints = get_delphioracle_datapoints(stablesale_itr->delphi_pair_name);

  bool found_point_with_median = false;
  for (auto itr = datapoints.begin(); itr != datapoints.end(); itr++) {
    if (itr->median == intended_delphi_median) {
        found_point_with_median = true;
        break;
    }
  }
  check(found_point_with_median,
  "No datapoint with the intended median was found. You likely took too long to confirm your transaction");


  //Using the price denoted in the stable symbol and the median price provided by the delphioracle,
  //the final price in the actual token is calculated

  double stable_price = (double) stablesale_itr->price.amount / pow(10, stablesale_itr->price.symbol.precision());
  auto pair_itr = delphioracle_pairs.find(stablesale_itr->delphi_pair_name.value);
  double stable_per_token = (double) intended_delphi_median / pow(10, pair_itr->quoted_precision);
  double token_price = stable_price / stable_per_token;

  config_s current_config = config.get();
  symbol token_symbol;
  for (DELPHIPAIR delphi_pair : current_config.supported_delphi_pairs) {
    if (delphi_pair.delphi_pair_name == stablesale_itr->delphi_pair_name) {
      token_symbol = delphi_pair.token_symbol;
    }
  }

  uint64_t final_price_amount = (uint64_t) (token_price * pow(10, token_symbol.precision()));
  asset final_price = asset(final_price_amount, token_symbol);

  internal_deduct_balance(
    buyer,
    final_price,
    string("Purchased Stable Sale")
  );

  internal_payout_sale(
    final_price,
    seller,
    stablesale_itr->maker_marketplace,
    taker_marketplace != name("") ? taker_marketplace : DEFAULT_MARKETPLACE_NAME,
    get_collection_author(asset_id, seller),
    stablesale_itr->collection_fee
  );

  action(
    permission_level{get_self(), name("active")},
    ATOMICASSETS_ACCOUNT,
    name("acceptoffer"),
    make_tuple(
      stablesale_itr->offer_id
    )
  ).send();

  internal_transfer_asset(buyer, stablesale_itr->asset_id, "You purchased this asset");

  seller_stablesales.erase(stablesale_itr);
}