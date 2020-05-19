#include <atomicmarket.hpp>
#include <atomicassets-interface.hpp>
#include <delphioracle-interface.hpp>

#include <math.h>


/**
*  Initializes the config table. Only needs to be called once when first deploying the contract
* 
*  @required_auth The contract itself
*/
ACTION atomicmarket::init() {
    require_auth(get_self());
    config.get_or_create(get_self(), config_s{});

    if (marketplaces.find(name("default").value) == marketplaces.end()) {
        marketplaces.emplace(get_self(), [&](auto &_marketplace) {
            _marketplace.marketplace_name = DEFAULT_MARKETPLACE_NAME;
            _marketplace.fee_account = DEFAULT_MARKETPLACE_FEE;
        });
    }
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
* Adds a stable pair that can be used for stable sales
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::adddelphi(
    name delphi_pair_name,
    symbol stable_symbol,
    symbol token_symbol
) {
    require_auth(get_self());

    auto pair_itr = delphioracle::pairs.require_find(delphi_pair_name.value,
        "The provided delphi_pair_name does not exist in the delphi oracle contract");

    check(is_symbol_supported(token_symbol), "The token symbol does not belong to a supported token");

    config_s current_config = config.get();

    for (DELPHIPAIR delphi_pair : current_config.supported_delphi_pairs) {
        check(delphi_pair.delphi_pair_name != delphi_pair_name,
            "There already exists a delphi pair using the provided delphi pair name");
    }

    current_config.supported_delphi_pairs.push_back({
        .delphi_pair_name = delphi_pair_name,
        .stable_symbol = stable_symbol,
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

    marketplaces.emplace(fee_account, [&](auto &_marketplace) {
        _marketplace.marketplace_name = marketplace_name;
        _marketplace.fee_account = fee_account;
    });
}




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

    atomicassets::assets_t seller_assets = atomicassets::get_assets(seller);
    seller_assets.require_find(asset_id,
        "You do not own the specified asset. You can only announce sales for assets that you currently own.");

    //If there still is a past sale entry for the asset, it is removed only if it was from a different seller
    auto sales_by_asset_id = sales.get_index<name("assetid")>();
    auto sale_itr = sales_by_asset_id.find(asset_id);
    if (sale_itr != sales_by_asset_id.end()) {
        check(sale_itr->seller != seller,
        "You have already announced a sale for this asset. You can cancel a sale using the cancelsale action.");

        sales_by_asset_id.erase(sale_itr);
    }
    

    check(is_symbol_supported(price.symbol), "The specified sale token is not supported.");
    check(price.amount >= 0, "The sale price must be positive");

    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

    config_s current_config = config.get();
    uint64_t sale_id = current_config.sale_counter++;
    config.set(current_config, get_self());

    sales.emplace(seller, [&](auto &_sale) {
        _sale.sale_id = sale_id;
        _sale.seller = seller;
        _sale.asset_id = asset_id;
        _sale.price = price;
        _sale.offer_id = -1;
        _sale.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
        _sale.collection_fee = get_collection_fee(asset_id, seller);
    });

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewsale"),
        make_tuple(
            sale_id,
            seller,
            asset_id,
            price,
            maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME
        )
    ).send();
}


/**
* Cancels a sale. The sale can both be active or inactive
* 
* @required_auth The sale's seller
*/
ACTION atomicmarket::cancelsale(
    uint64_t sale_id
) {
    auto sale_itr = sales.require_find(sale_id,
        "No sale with this sale_id exists");
    
    require_auth(sale_itr->seller);

    if (sale_itr->offer_id != -1) {
        if (atomicassets::offers.find(sale_itr->offer_id) != atomicassets::offers.end()) {
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

    sales.erase(sale_itr);
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
    uint64_t sale_id,
    name taker_marketplace
) {
    require_auth(buyer);

    auto sale_itr = sales.require_find(sale_id,
        "No sale with this sale_id exists");
    
    check(buyer != sale_itr->seller, "You can't purchase your own sale");

    check(sale_itr->offer_id != -1,
        "This sale is not active yet. The seller first has to create an atomicasset offer for this asset");

    check(atomicassets::offers.find(sale_itr->offer_id) != atomicassets::offers.end(),
        "The seller cancelled the atomicassets offer related to this sale");
    
    check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

    internal_deduct_balance(
        buyer,
        sale_itr->price,
        string("Purchased Sale")
    );

    internal_payout_sale(
        sale_itr->price,
        sale_itr->seller,
        sale_itr->maker_marketplace,
        taker_marketplace != name("") ? taker_marketplace : DEFAULT_MARKETPLACE_NAME,
        get_collection_author(sale_itr->asset_id, sale_itr->seller),
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

    sales.erase(sale_itr);
}




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

    atomicassets::assets_t seller_assets = atomicassets::get_assets(seller);
    seller_assets.require_find(asset_id,
        "You do not own the specified asset. You can only announce sales for assets that you currently own.");

    //If there still is a past stable sale entry for the asset, it is removed only if it was from a different seller
    auto stable_sales_by_asset_id = stable_sales.get_index<name("assetid")>();
    auto stable_sale_itr = stable_sales_by_asset_id.find(asset_id);
    if (stable_sale_itr != stable_sales_by_asset_id.end()) {
        check(stable_sale_itr->seller != seller,
        "You have already announced a stable sale for this asset. You can cancel a sale using the cancelstbk action.");

        stable_sales_by_asset_id.erase(stable_sale_itr);
    }

    config_s current_config = config.get();

    bool found_delphi_pair = false;
    for (DELPHIPAIR delphi_pair : current_config.supported_delphi_pairs) {
        if (delphi_pair.delphi_pair_name == delphi_pair_name) {
            found_delphi_pair = true;

            check(price.symbol == delphi_pair.stable_symbol,
                "The specified price uses a different symbol than the one required by the specified deplhi pair name");
            break;
        }
    }
    check(found_delphi_pair, "The specified delphi pair name is not supported");

    check(price.amount >= 0, "The sale price must be positive");

    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");
    
    uint64_t stable_sale_id = current_config.stable_sale_counter++;
    config.set(current_config, get_self());

    stable_sales.emplace(seller, [&](auto &_sale) {
        _sale.stable_sale_id = stable_sale_id;
        _sale.seller = seller;
        _sale.asset_id = asset_id;
        _sale.delphi_pair_name = delphi_pair_name;
        _sale.price = price;
        _sale.offer_id = -1;
        _sale.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
        _sale.collection_fee = get_collection_fee(asset_id, seller);
    });

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewstbl"),
        make_tuple(
            stable_sale_id,
            seller,
            asset_id,
            delphi_pair_name,
            price,
            maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME
        )
    ).send();
}


/**
* Cancels a stable sale. The stable sale can both be active or inactive
* 
* @required_auth The stable sale's seller
*/
ACTION atomicmarket::cancelstbl(
    uint64_t stable_sale_id
) {
    auto stable_sale_itr = stable_sales.require_find(stable_sale_id,
        "No stable sale with this stable_sale_id exists");
    
    require_auth(stable_sale_itr->seller);

    if (stable_sale_itr->offer_id != -1) {
        if (atomicassets::offers.find(stable_sale_itr->offer_id) != atomicassets::offers.end()) {
            //Cancels the atomicassets offer for this stable sale for convenience
            action(
                permission_level{get_self(), name("active")},
                ATOMICASSETS_ACCOUNT,
                name("declineoffer"),
                make_tuple(
                    stable_sale_itr->offer_id
                )
            ).send();
        }
    }

    stable_sales.erase(stable_sale_itr);
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
    uint64_t stable_sale_id,
    uint64_t intended_delphi_median,
    name taker_marketplace
) {
    require_auth(buyer);

    auto stable_sale_itr = stable_sales.require_find(stable_sale_id,
        "No stable sale with this stable_sale_id exists");
    
    check(buyer != stable_sale_itr->seller, "You can't purchase your own stable sale");

    check(stable_sale_itr->offer_id != -1,
        "This sale is not active yet. The seller first has to create an atomicasset offer for this asset");

    check(atomicassets::offers.find(stable_sale_itr->offer_id) != atomicassets::offers.end(),
        "The seller cancelled the atomicassets offer related to this stable sale");

    check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

    delphioracle::datapoints_t datapoints = delphioracle::get_datapoints(stable_sale_itr->delphi_pair_name);

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

    double stable_price = (double) stable_sale_itr->price.amount / pow(10, stable_sale_itr->price.symbol.precision());
    auto pair_itr = delphioracle::pairs.find(stable_sale_itr->delphi_pair_name.value);
    double stable_per_token = (double) intended_delphi_median / pow(10, pair_itr->quoted_precision);
    double token_price = stable_price / stable_per_token;

    config_s current_config = config.get();
    symbol token_symbol;
    for (DELPHIPAIR delphi_pair : current_config.supported_delphi_pairs) {
        if (delphi_pair.delphi_pair_name == stable_sale_itr->delphi_pair_name) {
            token_symbol = delphi_pair.token_symbol;
            break;
        }
    }

    uint64_t final_price_amount = (uint64_t)(token_price * pow(10, token_symbol.precision()));
    asset final_price = asset(final_price_amount, token_symbol);

    internal_deduct_balance(
        buyer,
        final_price,
        string("Purchased Stable Sale")
    );

    internal_payout_sale(
        final_price,
        stable_sale_itr->seller,
        stable_sale_itr->maker_marketplace,
        taker_marketplace != name("") ? taker_marketplace : DEFAULT_MARKETPLACE_NAME,
        get_collection_author(stable_sale_itr->asset_id, stable_sale_itr->seller),
        stable_sale_itr->collection_fee
    );

    action(
        permission_level{get_self(), name("active")},
        ATOMICASSETS_ACCOUNT,
        name("acceptoffer"),
        make_tuple(
            stable_sale_itr->offer_id
        )
    ).send();

    internal_transfer_asset(buyer, stable_sale_itr->asset_id, "You purchased this asset");

    stable_sales.erase(stable_sale_itr);
}




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

    atomicassets::assets_t seller_assets = atomicassets::get_assets(seller);
    seller_assets.require_find(asset_id,
        "You do not own the specified asset. You can only announce auctions for assets that you currently own.");
    
    auto auctions_by_asset_id = auctions.get_index<name("assetid")>();
    auto auction_itr = auctions_by_asset_id.find(asset_id);
    //Goes through all auctions for the specified asset_id, skips those auctions that are already finished
    //but not claimed by both parties yet, and deletes a previously announced auction that is not finished yet
    while (auction_itr != auctions_by_asset_id.end() && auction_itr->asset_id == asset_id) {
        //Skip finished auctions
        if (current_time_point().sec_since_epoch() < auction_itr->end_time) {
            check(auction_itr->seller != seller,
                "You have already announced am auction for this asset");
            //This can be done without checking if the auction is active, because if the auction was active,
            //the asset would be owned by the atomicmarket account
            auctions_by_asset_id.erase(auction_itr);
        }

        auction_itr++;
    }

    check(is_symbol_supported(starting_bid.symbol), "The specified starting bid token is not supported.");
    check(starting_bid.amount >= 0, "The starting bid must be positive");

    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

    config_s current_config = config.get();
    check(duration <= current_config.maximum_auction_duration,
        "The specified duration is longer than the maximum auction duration");
    
    uint64_t auction_id = current_config.auction_counter++;
    config.set(current_config, get_self());

    auctions.emplace(seller, [&](auto &_auction) {
        _auction.auction_id = auction_id;
        _auction.seller = seller;
        _auction.asset_id = asset_id;
        _auction.end_time = current_time_point().sec_since_epoch() + duration;
        _auction.is_active = false;
        _auction.current_bid = starting_bid;
        _auction.current_bidder = name("");
        _auction.claimed_by_seller = false;
        _auction.claimed_by_buyer = false;
        _auction.maker_marketplace = maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME;
        _auction.taker_marketplace = name("");
        _auction.collection_fee = get_collection_fee(asset_id, seller);
        _auction.collection_author = get_collection_author(asset_id, seller);
    });

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewauct"),
        make_tuple(
            auction_id,
            seller,
            asset_id,
            starting_bid,
            duration,
            maker_marketplace != name("") ? maker_marketplace : DEFAULT_MARKETPLACE_NAME
        )
    ).send();
}


/**
* Cancels an auction. If the auction is active, it must not have any bids yet.
* Auctions with bids can't be cancelled.
* 
* @required_auth seller
*/
ACTION atomicmarket::cancelauct(
    uint64_t auction_id
) {
    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");
    
    require_auth(auction_itr->seller);

    if (auction_itr->is_active) {
        check(auction_itr->current_bidder == name(""),
            "This auction already has a bid. Auctions with bids can't be cancelled");

        internal_transfer_asset(auction_itr->seller, auction_itr->asset_id, string("Cancelled Auction"));
    }

    auctions.erase(auction_itr);
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
    uint64_t auction_id,
    asset bid,
    name taker_marketplace
) {
    require_auth(bidder);

    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");

    check(bidder != auction_itr->seller, "You can't bid on your own auction");

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
        check((double) bid.amount >=
              (double) auction_itr->current_bid.amount * (1.0 + current_config.minimum_bid_increase),
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

    auctions.modify(auction_itr, same_payer, [&](auto &_auction) {
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
    uint64_t auction_id
) {
    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");
    
    require_auth(auction_itr->current_bidder);

    check(auction_itr->is_active, "The auction is not active");

    check(auction_itr->end_time < current_time_point().sec_since_epoch(),
        "The auction is not finished yet");

    check(auction_itr->current_bidder != name(""),
        "The auction does not have any bids");

    internal_transfer_asset(
        auction_itr->current_bidder,
        auction_itr->asset_id,
        string("You won an auction")
    );

    if (auction_itr->claimed_by_seller) {
        auctions.erase(auction_itr);
    } else {
        auctions.modify(auction_itr, same_payer, [&](auto &_auction) {
            _auction.claimed_by_buyer = true;
        });
    }
}


/**
* Claims the highest bid of an auction for the seller and also gives a cut to the marketplaces and the collection
* 
* If the auction has no bids, use the cancelauct action instead
* 
* @required_auth The auction's seller
*/
ACTION atomicmarket::auctclaimsel(
    uint64_t auction_id
) {
    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");
    
    require_auth(auction_itr->seller);

    check(auction_itr->is_active, "The auction is not active");

    check(auction_itr->end_time < current_time_point().sec_since_epoch(),
        "The auction is not finished yet");

    check(auction_itr->current_bidder != name(""),
        "The auction does not have any bids");

    internal_payout_sale(
        auction_itr->current_bid,
        auction_itr->seller,
        auction_itr->maker_marketplace,
        auction_itr->taker_marketplace,
        auction_itr->collection_author,
        auction_itr->collection_fee
    );

    if (auction_itr->claimed_by_buyer) {
        auctions.erase(auction_itr);
    } else {
        auctions.modify(auction_itr, same_payer, [&](auto &_auction) {
            _auction.claimed_by_seller = true;
        });
    }
}




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
    vector <uint64_t> asset_ids,
    string memo
) {
    if (to != get_self()) {
        return;
    }

    if (memo == "auction") {
        check(asset_ids.size() == 1, "You must transfer exactly one asset per action for an auction");

        uint64_t asset_id = asset_ids[0];

        auto auctions_by_asset_id = auctions.get_index<name("assetid")>();
        auto auction_itr = auctions_by_asset_id.find(asset_id);

        //Find the announced, non-fished auction (if there is one)
        while (true) {
            check(auction_itr != auctions_by_asset_id.end() && auction_itr->asset_id == asset_id,
                "No announced, non-finished auction for this asset exists");
            if (current_time_point().sec_since_epoch() < auction_itr->end_time) {
                break;
            }
            auction_itr++;
        }
        
        check(auction_itr->seller == from,
            "The announced auction for this asset is from another seller");

        auctions_by_asset_id.modify(auction_itr, same_payer, [&](auto &_auction) {
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
    vector <uint64_t> sender_asset_ids,
    vector <uint64_t> recipient_asset_ids,
    string memo
) {
    if (recipient != get_self()) {
        return;
    }

    if (memo == "sale") {
        check(sender_asset_ids.size() == 1, "You must offer exactly one asset per action for a sale");
        check(recipient_asset_ids.size() == 0, "You must not ask for any assets in return in a sale offer");

        uint64_t asset_id = sender_asset_ids[0];

        auto sales_by_asset_id = sales.get_index<name("assetid")>();
        auto sale_itr = sales_by_asset_id.require_find(asset_id,
            "No sale for this asset exists");
        
        check(sale_itr->seller == sender,
            "The announced sale for this asset is from another seller");

        sales_by_asset_id.modify(sale_itr, same_payer, [&](auto &_sale) {
            _sale.offer_id = offer_id;
        });

    } else if (memo == "stablesale") {
        check(sender_asset_ids.size() == 1, "You must offer exactly one asset per action for a sale");
        check(recipient_asset_ids.size() == 0, "You must not ask for any assets in return in a sale offer");

        uint64_t asset_id = sender_asset_ids[0];
        auto stable_sales_by_asset_id = stable_sales.get_index<name("assetid")>();
        auto stable_sale_itr = stable_sales_by_asset_id.require_find(asset_id,
            "You have not created have a stable sale for this asset");

        stable_sales_by_asset_id.modify(stable_sale_itr, same_payer, [&](auto &_stable_sale) {
            _stable_sale.offer_id = offer_id;
        });

    } else {
        check(false, "Invalid memo");
    }
}




ACTION atomicmarket::lognewsale(
    uint64_t sale_id,
    name seller,
    uint64_t asset_id,
    asset price,
    name maker_marketplace
) {
    require_auth(get_self());

    require_recipient(seller);
}

ACTION atomicmarket::lognewstbl(
    uint64_t stable_sale_id,
    name seller,
    uint64_t asset_id,
    name delphi_pair_name,
    asset price,
    name maker_marketplace
) {
    require_auth(get_self());

    require_recipient(seller);
}

ACTION atomicmarket::lognewauct(
    uint64_t auction_id,
    name seller,
    uint64_t asset_id,
    asset starting_bid,
    uint32_t duration,
    name maker_marketplace
) {
    require_auth(get_self());

    require_recipient(seller);
}


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
    atomicassets::assets_t owner_assets = atomicassets::get_assets(asset_owner);
    auto asset_itr = owner_assets.find(asset_id);

    auto collection_itr = atomicassets::collections.find(asset_itr->collection_name.value);
    return collection_itr->author;
}


/**
* Gets the fee defined by a collection in the atomicassets contract
*/
double atomicmarket::get_collection_fee(uint64_t asset_id, name asset_owner) {
    atomicassets::assets_t owner_assets = atomicassets::get_assets(asset_owner);
    auto asset_itr = owner_assets.find(asset_id);

    auto collection_itr = atomicassets::collections.find(asset_itr->collection_name.value);
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
    uint64_t maker_cut_amount = (uint64_t)(current_config.maker_market_fee * (double) quantity.amount);
    uint64_t taker_cut_amount = (uint64_t)(current_config.taker_market_fee * (double) quantity.amount);
    uint64_t collection_cut_amount = (uint64_t)(collection_fee * (double) quantity.amount);
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

    vector <asset> quantities;
    if (balance_itr == balances.end()) {
        //No balance table row exists yet
        quantities = {quantity};
        balances.emplace(get_self(), [&](auto &_balance) {
            _balance.owner = user;
            _balance.quantities = quantities;
        });

    } else {
        //A balance table row already exists for user
        quantities = balance_itr->quantities;

        bool found_token = false;
        for (asset &token : quantities) {
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

        balances.modify(balance_itr, get_self(), [&](auto &_balance) {
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

    vector <asset> user_balance = balance_itr->quantities;

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
        balances.modify(balance_itr, get_self(), [&](auto &_balance) {
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
    vector <uint64_t> assets = {asset_id};

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