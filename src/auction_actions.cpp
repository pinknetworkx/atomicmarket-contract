#include <atomicmarket.hpp>

/**
* Create an auction listing
* For the auction to become active, the seller needs to use the atomicassets transfer action to transfer the asset
* to the atomicmarket contract with the memo "auction"
* 
* duration is in seconds
* 
* @required_auth seller
*/
ACTION atomicmarket::announceauct(
  name seller,
  uint64_t asset_id,
  asset starting_bid,
  uint32_t duration,
  name maker_marketplace
) {
  require_auth(seller);

  atomicassets_assets_t seller_assets = get_atomicassets_assets(seller);
  seller_assets.require_find(asset_id,
  "You do not own the specified asset. You can only announce auctions for assets that you currently own.");

  auctions_t seller_auctions = get_auctions(seller);
  check(seller_auctions.find(asset_id) == seller_auctions.end(),
  "You have already announced an auction for this asset. You can cancel an auction using the cancelauct action.");

  check(is_symbol_supported(starting_bid.symbol), "The specified starting bid token is not supported.");
  check(starting_bid.amount >= 0, "The starting bid must be positive");

  check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

  config_s current_config = config.get();
  check(duration <= current_config.maximum_auction_duration,
  "The specified duration is longer than the maximum auction duration");

  seller_auctions.emplace(seller, [&](auto& _auction) {
    _auction.asset_id = asset_id;
    _auction.end_time = current_time_point().sec_since_epoch() + duration;
    _auction.current_bid = starting_bid;
    _auction.current_bidder = name("");
    _auction.is_active = false;
    _auction.claimed_by_seller = false;
    _auction.claimed_by_buyer = false;
    _auction.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
    _auction.taker_marketplace = name("");
    _auction.collection_fee = get_collection_fee(asset_id, seller);
    _auction.collection_author = get_collection_author(asset_id, seller);
  });
}




/**
* Cancels an auction. If the auction is active, it must not have any bids yet.
* Auctions with bids can't be cancelled.
* 
* @required_auth seller
*/
ACTION atomicmarket::cancelauct(
  name seller,
  uint64_t asset_id
) {
  require_auth(seller);

  auctions_t seller_auctions = get_auctions(seller);
  auto auction_itr = seller_auctions.require_find(asset_id,
  "The seller does not have an auction with this asset_id");

  if (auction_itr->is_active) {
    check(auction_itr->current_bidder == name(""),
    "This auction already has a bid. Auctions with bids can't be cancelled");

    internal_transfer_asset(seller, asset_id, string("Cancelled Auction"));
  }

  seller_auctions.erase(auction_itr);
}




/**
* Places a bid on an auction
* The bid is deducted from the buyer's balance
* If a higher bid gets placed by someone else, the original bid will be refunded to the original buyer's balance
* 
* @required_auth bidder
*/
ACTION atomicmarket::auctionbid(
  name bidder,
  name seller,
  uint64_t asset_id,
  asset bid,
  name taker_marketplace
) {
  require_auth(bidder);

  check(bidder != seller, "You can't bid on your own auction");

  auctions_t seller_auctions = get_auctions(seller);
  auto auction_itr = seller_auctions.require_find(asset_id,
  "The specified seller does not have an auction for this asset");

  check(auction_itr->is_active,
  "The auction is not yet active. The seller first needs to transfer the asset to the atomicmarket account");

  check(current_time_point().sec_since_epoch() < auction_itr->end_time,
  "The auction is already finsihed");

  check(bid.symbol == auction_itr->current_bid.symbol,
  "The bid uses a different symbol than the current auction bid");

  config_s current_config = config.get();
  if (auction_itr->current_bidder == name("")) {
    check(bid.amount >= auction_itr->current_bid.amount,
    "The bid must be at least as high as the minimum bid");
  } else {
    check((double) bid.amount >= (double) auction_itr->current_bid.amount * (1.0 + current_config.minimum_bid_increase),
    "The relative increase is less than the minimum bid increase specified in the config");
  }
  

  if (auction_itr->current_bidder != name("")) {
    internal_add_balance(
      auction_itr->current_bidder,
      auction_itr->current_bid,
      string("Auction outbid refund")
    );
  }

  internal_deduct_balance(
    bidder,
    bid,
    string("Auction bid")
  );

  check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

  seller_auctions.modify(auction_itr, same_payer, [&](auto& _auction) {
    _auction.current_bid = bid;
    _auction.current_bidder = bidder;
    _auction.taker_marketplace = taker_marketplace != name("") ? taker_marketplace : DEFAULT_MARKETPLACE_NAME;
  });
}




/**
* Claims the asset for the highest bidder of an auction
* 
* @required_auth The highest bidder of the auction
*/
ACTION atomicmarket::auctclaimbuy(
  name seller,
  uint64_t asset_id
) {
  auctions_t seller_auctions = get_auctions(seller);
  auto auction_itr = seller_auctions.require_find(asset_id,
  "The specified seller has no auction with this asset");

  check(auction_itr->is_active, "The auction is not active");

  check(auction_itr->end_time < current_time_point().sec_since_epoch(),
  "The auction is not finished yet");

  check(auction_itr->current_bidder != name(""),
  "The auction does not have any bids");

  require_auth(auction_itr->current_bidder);

  internal_transfer_asset(
    auction_itr->current_bidder,
    asset_id,
    string("You won an auction")
  );

  if (auction_itr->claimed_by_seller) {
    seller_auctions.erase(auction_itr);
  } else {
    seller_auctions.modify(auction_itr, same_payer, [&](auto& _auction) {
      _auction.claimed_by_buyer = true;
    });
  }
}





/**
* Claims the highest bid of an auction for the seller and also gives a cut to the marketplaces and the collection
* 
* If the auction has no bids, use the cancelauct action instead
* 
* @required_auth seller
*/
ACTION atomicmarket::auctclaimsel(
  name seller,
  uint64_t asset_id
) {
  require_auth(seller);

  auctions_t seller_auctions = get_auctions(seller);
  auto auction_itr = seller_auctions.require_find(asset_id,
  "The specified seller has no auction with this asset");

  check(auction_itr->is_active, "The auction is not active");

  check(auction_itr->end_time < current_time_point().sec_since_epoch(),
  "The auction is not finished yet");

  check(auction_itr->current_bidder != name(""),
  "The auction does not have any bids");

  internal_payout_sale(
    auction_itr->current_bid,
    seller,
    auction_itr->maker_marketplace,
    auction_itr->taker_marketplace,
    auction_itr->collection_author,
    auction_itr->collection_fee
  );

  if (auction_itr->claimed_by_buyer) {
    seller_auctions.erase(auction_itr);
  } else {
    seller_auctions.modify(auction_itr, same_payer, [&](auto& _auction) {
      _auction.claimed_by_seller = true;
    });
  }
}