#include <atomicmarket.hpp>

/**
* Create a sale listing
* For the sale to become active, the seller needs to create an atomicassets offer from them to the atomicmarket
* account, offering (only) the asset to be sold with the memo "sale"
* 
* @required_auth seller
*/
ACTION atomicmarket::announcesale(
  name seller,
  uint64_t asset_id,
  asset price,
  name maker_marketplace
) {
  require_auth(seller);

  atomicassets_assets_t seller_assets = get_atomicassets_assets(seller);
  seller_assets.require_find(asset_id,
  "You do not own the specified asset. You can only announce sales for assets that you currently own.");

  sales_t seller_sales = get_sales(seller);
  check(seller_sales.find(asset_id) == seller_sales.end(),
  "You have already announced a sale for this asset. You can cancel a sale using the cancelsale action.");

  check(is_symbol_supported(price.symbol), "The specified sale token is not supported.");
  check(price.amount >= 0, "The sale price must be positive");

  check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

  seller_sales.emplace(seller, [&](auto& _sale) {
    _sale.asset_id = asset_id;
    _sale.price = price;
    _sale.offer_id = -1;
    _sale.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
    _sale.collection_fee = get_collection_fee(asset_id, seller);
  });
}




/**
* Cancels a sale. The sale can both be active or inactive
* 
* @required_auth seller
*/
ACTION atomicmarket::cancelsale(
  name seller,
  uint64_t asset_id
) {
  require_auth(seller);

  sales_t seller_sales = get_sales(seller);
  auto sale_itr = seller_sales.require_find(asset_id,
  "The seller does not have a sale with this asset_id");

  if (sale_itr->offer_id != -1) {
    if (atomicassets_offers.find(sale_itr->offer_id) != atomicassets_offers.end()) {
      //Cancels the atomicassets offer for this sale for convenience
      action(
        permission_level{get_self(), name("active")},
        ATOMICASSETS_ACCOUNT,
        name("declineoffer"),
        make_tuple(
          sale_itr->offer_id
        )
      ).send();
    }
  }

  seller_sales.erase(sale_itr);
}




/**
* Purchases an asset that is for sale.
* The sale price is deducted from the buyer's balance and added to the seller's balance
* 
* The intended_price parameter must match the sale's price. This is to prevent possible attacks where a seller could
* try to increase the price of the sale very shortly before the buyer sends the buy auction
* 
* @required_auth buyer
*/
ACTION atomicmarket::purchasesale(
  name buyer,
  name seller,
  uint64_t asset_id,
  asset intended_price,
  name taker_marketplace
) {
  require_auth(buyer);

  check(buyer != seller, "You can't purchase your own sale");

  sales_t seller_sales = get_sales(seller);
  auto sale_itr = seller_sales.require_find(asset_id,
  "The specified seller does not have a sale for this asset");

  check(sale_itr->offer_id != -1,
  "This sale is not active yet. The seller first has to create an atomicasset offer for this asset");

  check(atomicassets_offers.find(sale_itr->offer_id) != atomicassets_offers.end(),
  "The seller cancelled the atomicassets offer related to this sale");

  check(sale_itr->price.symbol == intended_price.symbol,
  "The intended price uses a different symbol than the sale price");

  check(sale_itr->price == intended_price, "The intended price does not match the sale's price");

  check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

  internal_deduct_balance(
    buyer,
    sale_itr->price,
    string("Purchased Sale")
  );

  internal_payout_sale(
    sale_itr->price,
    seller,
    sale_itr->maker_marketplace,
    taker_marketplace != name("") ? taker_marketplace : DEFAULT_MARKETPLACE_NAME,
    get_collection_author(asset_id, seller),
    sale_itr->collection_fee
  );

  action(
    permission_level{get_self(), name("active")},
    ATOMICASSETS_ACCOUNT,
    name("acceptoffer"),
    make_tuple(
      sale_itr->offer_id
    )
  ).send();

  internal_transfer_asset(buyer, sale_itr->asset_id, "You purchased this asset");

  seller_sales.erase(sale_itr);
}