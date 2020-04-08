#include <atomicmarket.hpp>


/**
*  This function is called when a transfer receipt from any token contract is sent to the atomicmarket contract
*  It handels deposits and adds the transferred tokens to the sender's balance table row
*/
void atomicmarket::receive_token_transfer(name from, name to, asset quantity, string memo) {
  if (to != get_self()) {
    return;
  }

  check(is_token_supported(get_first_receiver(), quantity.symbol), "The transferred token is not supported");

  if (memo == "deposit") {
    internal_add_balance(from, quantity, string("Deposit"));
  } else {
    check(false, "invalid memo");
  }
}







/**
* This function is called when a "transfer" action receipt from the atomicassets contract is sent to the atomicmarket
* contract. It handles receiving assets for auctions.
*/
void atomicmarket::receive_asset_transfer(
  name from,
  name to,
  vector<uint64_t> asset_ids,
  string memo
) {
  if (to != get_self()) {
    return;
  }

  if (memo == "auction") {
    check(asset_ids.size() == 1, "You must transfer exactly one asset per action for an auction");

    uint64_t asset_id = asset_ids[0];
    auctions_t from_auctions = get_auctions(from);
    auto auction_itr = from_auctions.require_find(asset_id,
    "You have not created have an auction for this asset");

    check(current_time_point().sec_since_epoch() < auction_itr->end_time,
    "The auction is already finished");

    from_auctions.modify(auction_itr, same_payer, [&](auto& _auction) {
      _auction.is_active = true;
    });

  } else {
    check(false, "Invalid memo");
  }
}




/**
* This function is called when a "lognewoffer" action receipt from the atomicassets contract is sent to the 
* atomicmarket contract. It handles receiving offers for sales.
*/
void atomicmarket::receive_asset_offer(
  uint64_t offer_id,
  name sender,
  name recipient,
  vector<uint64_t> sender_asset_ids,
  vector<uint64_t> recipient_asset_ids,
  string memo
) {
  if (recipient != get_self()) {
    return;
  }

  if (memo == "sale") {
    check(sender_asset_ids.size() == 1, "You must offer exactly one asset per action for a sale");
    check(recipient_asset_ids.size() == 0, "You must not ask for any assets in return in a sale offer");

    uint64_t asset_id = sender_asset_ids[0];
    sales_t sender_sales = get_sales(sender);
    auto sale_itr = sender_sales.require_find(asset_id,
    "You have not created have a sale for this asset");

    sender_sales.modify(sale_itr, same_payer, [&](auto& _sale) {
      _sale.offer_id = offer_id;
    });

  } else {
    check(false, "Invalid memo");
  }
}


