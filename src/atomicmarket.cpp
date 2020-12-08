#include <atomicmarket.hpp>

#include <math.h>


/**
* Initializes the config table. Only needs to be called once when first deploying the contract
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::init() {
    require_auth(get_self());
    config.get_or_create(get_self(), config_s{});

    if (marketplaces.find(name("").value) == marketplaces.end()) {
        marketplaces.emplace(get_self(), [&](auto &_marketplace) {
            _marketplace.marketplace_name = name("");
            _marketplace.creator = DEFAULT_MARKETPLACE_CREATOR;
        });
    }
}


/**
* Converts the now deprecated sale and auction counters in the config singleton
* into using the counters table
* 
* Calling this only is necessary when upgrading the contract from a lower version to 1.3.0
* When deploying a fresh contract, this action can be ignored completely
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::convcounters() {
    require_auth(get_self());

    config_s current_config = config.get();

    check(current_config.sale_counter != 0 && current_config.auction_counter != 0,
        "The sale or auction counters have already been converted");

    counters.emplace(get_self(), [&](auto &_counter) {
        _counter.counter_name = name("sale");
        _counter.counter_value = current_config.sale_counter;
    });
    current_config.sale_counter = 0;

    counters.emplace(get_self(), [&](auto &_counter) {
        _counter.counter_name = name("auction");
        _counter.counter_value = current_config.auction_counter;
    });
    current_config.auction_counter = 0;

    config.set(current_config, get_self());
}


/**
* Sets the minimum bid increase compared to the previous bid
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::setminbidinc(double minimum_bid_increase) {
    require_auth(get_self());
    check(minimum_bid_increase > 0, "The bid increase must be greater than 0");

    config_s current_config = config.get();
    current_config.minimum_bid_increase = minimum_bid_increase;
    config.set(current_config, get_self());
}


/**
* Sets the version for the config table
* 
* @required_auth The contract itself
*/
ACTION atomicmarket::setversion(string new_version) {
    require_auth(get_self());

    config_s current_config = config.get();
    current_config.version = new_version;

    config.set(current_config, get_self());
}


/**
* Adds a token that can be used to sell assets for
* 
* @required_auth The contract itself
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
    bool invert_delphi_pair,
    symbol listing_symbol,
    symbol settlement_symbol
) {
    require_auth(get_self());

    check(listing_symbol != settlement_symbol,
        "Listing symbol and settlement symbol must be different");

    auto pair_itr = delphioracle::pairs.require_find(delphi_pair_name.value,
        "The provided delphi_pair_name does not exist in the delphi oracle contract");
    if (!invert_delphi_pair) {
        check(listing_symbol.precision() == pair_itr->quote_symbol.precision(),
            "The listing symbol precision needs to be equal to the delphi quote smybol precision for non inverted pairs");
        check(settlement_symbol.precision() == pair_itr->base_symbol.precision(),
            "The settlement symbol precision needs to be equal to the delphi base smybol precision for non inverted pairs");
    } else {
        check(listing_symbol.precision() == pair_itr->base_symbol.precision(),
            "The listing symbol precision needs to be equal to the delphi base smybol precision for inverted pairs");
        check(settlement_symbol.precision() == pair_itr->quote_symbol.precision(),
            "The settlement symbol precision needs to be equal to the delphi quote smybol precision for inverted pairs");
    }

    check(!is_symbol_pair_supported(listing_symbol, settlement_symbol),
        "There already exists a symbol pair with the specified listing - settlement symbol combination");

    check(is_symbol_supported(settlement_symbol), "The settlement symbol does not belong to a supported token");


    config_s current_config = config.get();

    current_config.supported_symbol_pairs.push_back({
        .listing_symbol = listing_symbol,
        .settlement_symbol = settlement_symbol,
        .delphi_pair_name = delphi_pair_name,
        .invert_delphi_pair = invert_delphi_pair
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

    check(maker_market_fee >= 0 && taker_market_fee >= 0,
        "Market fees need to be at least 0");

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
* @required_auth creator
*/
ACTION atomicmarket::regmarket(
    name creator,
    name marketplace_name
) {
    require_auth(creator);

    name marketplace_name_suffix = marketplace_name.suffix();

    if (is_account(marketplace_name)) {
        check(has_auth(marketplace_name),
            "When the marketplace has the name of an existing account, its authorization is required");
    } else {
        if (marketplace_name_suffix != marketplace_name) {
            check(has_auth(marketplace_name_suffix),
                "When the marketplace name has a suffix, the suffix authorization is required");
        } else {
            check(marketplace_name.length() == 12,
                "Without special authorization, marketplace names must be 12 characters long");
        }
    }

    check(marketplaces.find(marketplace_name.value) == marketplaces.end(),
        "A marketplace with this name already exists");


    marketplaces.emplace(creator, [&](auto &_marketplace) {
        _marketplace.marketplace_name = marketplace_name;
        _marketplace.creator = creator;
    });
}


/**
* Withdraws a token from a users balance. The specified token is then transferred to the user.
* 
* @required_auth owner
*/
ACTION atomicmarket::withdraw(
    name owner,
    asset token_to_withdraw
) {
    require_auth(owner);

    internal_withdraw_tokens(owner, token_to_withdraw, "AtomicMarket Withdrawal");
}


/**
* Create a sale listing
* For the sale to become active, the seller needs to create an atomicassets offer from them to the atomicmarket
* account, offering (only) the assets to be sold with the memo "sale"
* 
* @required_auth seller
*/
ACTION atomicmarket::announcesale(
    name seller,
    vector <uint64_t> asset_ids,
    asset listing_price,
    symbol settlement_symbol,
    name maker_marketplace
) {
    require_auth(seller);

    name assets_collection_name = get_collection_and_check_assets(seller, asset_ids);


    checksum256 asset_ids_hash = hash_asset_ids(asset_ids);

    auto sales_by_hash = sales.get_index <name("assetidshash")>();
    auto sale_itr = sales_by_hash.find(asset_ids_hash);

    while (sale_itr != sales_by_hash.end()) {
        if (asset_ids_hash != sale_itr->asset_ids_hash()) {
            break;
        }

        check(sale_itr->seller != seller,
            "You have already announced a sale for these assets. You can cancel a sale using the cancelsale action.");

        sale_itr++;
    }


    if (listing_price.symbol == settlement_symbol) {
        check(is_symbol_supported(listing_price.symbol), "The specified listing symbol is not supported.");
    } else {
        check(is_symbol_pair_supported(listing_price.symbol, settlement_symbol),
            "The specified listing - settlement symbol combination is not supported");
    }


    check(listing_price.amount > 0, "The sale price must be greater than zero");

    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

    double collection_fee = get_collection_fee(assets_collection_name);
    check(collection_fee <= atomicassets::MAX_MARKET_FEE,
        "The collection fee is too high. This should have been prevented by the atomicassets contract");

    uint64_t sale_id = consume_counter(name("sale"));

    sales.emplace(seller, [&](auto &_sale) {
        _sale.sale_id = sale_id;
        _sale.seller = seller;
        _sale.asset_ids = asset_ids;
        _sale.offer_id = -1;
        _sale.listing_price = listing_price;
        _sale.settlement_symbol = settlement_symbol;
        _sale.maker_marketplace = maker_marketplace;
        _sale.collection_name = assets_collection_name;
        _sale.collection_fee = collection_fee;
    });


    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewsale"),
        make_tuple(
            sale_id,
            seller,
            asset_ids,
            listing_price,
            settlement_symbol,
            maker_marketplace,
            assets_collection_name,
            collection_fee
        )
    ).send();
}


/**
* Cancels a sale. The sale can both be active or inactive
* 
* If the sale is invalid (offer for the sale was cancelled or the seller does not own at least one
* of the assets on sale, this action can be called without the authorization of the seller
* 
* @required_auth The sale's seller
*/
ACTION atomicmarket::cancelsale(
    uint64_t sale_id
) {
    auto sale_itr = sales.require_find(sale_id,
        "No sale with this sale_id exists");


    bool is_sale_invalid = false;

    if (sale_itr->offer_id != -1) {
        if (atomicassets::offers.find(sale_itr->offer_id) == atomicassets::offers.end()) {
            is_sale_invalid = true;
        }
    }

    atomicassets::assets_t seller_assets = atomicassets::get_assets(sale_itr->seller);
    for (uint64_t asset_id : sale_itr->asset_ids) {
        if (seller_assets.find(asset_id) == seller_assets.end()) {
            is_sale_invalid = true;
            break;
        }
    }

    check(is_sale_invalid || has_auth(sale_itr->seller),
        "The sale is not invalid, therefore the authorization of the seller is needed to cancel it");


    if (sale_itr->offer_id != -1) {
        if (atomicassets::offers.find(sale_itr->offer_id) != atomicassets::offers.end()) {
            //Cancels the atomicassets offer for this sale for convenience
            action(
                permission_level{get_self(), name("active")},
                atomicassets::ATOMICASSETS_ACCOUNT,
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
* intended_delphi_median is only relevant if the sale uses a delphi pairing. Otherwise it is not checked.
* 
* @required_auth buyer
*/
ACTION atomicmarket::purchasesale(
    name buyer,
    uint64_t sale_id,
    uint64_t intended_delphi_median,
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


    asset sale_price;

    if (sale_itr->listing_price.symbol == sale_itr->settlement_symbol) {
        check(intended_delphi_median == 0, "intended delphi median needs to be 0 for non delphi sales");
        sale_price = sale_itr->listing_price;

    } else {
        SYMBOLPAIR symbol_pair = require_get_symbol_pair(sale_itr->listing_price.symbol, sale_itr->settlement_symbol);

        delphioracle::datapoints_t datapoints = delphioracle::get_datapoints(symbol_pair.delphi_pair_name);

        bool found_point_with_median = false;
        for (auto itr = datapoints.begin(); itr != datapoints.end(); itr++) {
            if (itr->median == intended_delphi_median) {
                found_point_with_median = true;
                break;
            }
        }
        check(found_point_with_median,
            "No datapoint with the intended median was found. You likely took too long to confirm your transaction");


        //Using the price denoted in the listing symbol and the median price provided by the delphioracle,
        //the final price in the settlement token is calculated
        auto pair_itr = delphioracle::pairs.find(symbol_pair.delphi_pair_name.value);

        uint64_t settlement_price_amount;

        if (!symbol_pair.invert_delphi_pair) {
            //Normal
            settlement_price_amount = (double) sale_itr->listing_price.amount / (double) intended_delphi_median * pow(
                10, pair_itr->quoted_precision + sale_itr->settlement_symbol.precision() -
                    sale_itr->listing_price.symbol.precision()
            );
        } else {
            //Inverted
            settlement_price_amount = (double) sale_itr->listing_price.amount * (double) intended_delphi_median * pow(
                10, -pair_itr->quoted_precision + sale_itr->settlement_symbol.precision() -
                    sale_itr->listing_price.symbol.precision()
            );
        }

        sale_price = asset(settlement_price_amount, sale_itr->settlement_symbol);

    }


    internal_decrease_balance(
        buyer,
        sale_price
    );

    internal_payout_sale(
        sale_price,
        sale_itr->seller,
        sale_itr->maker_marketplace,
        taker_marketplace,
        get_collection_author(sale_itr->collection_name),
        sale_itr->collection_fee,
        "AtomicMarket Sale Payout - ID #" + to_string(sale_id)
    );

    action(
        permission_level{get_self(), name("active")},
        atomicassets::ATOMICASSETS_ACCOUNT,
        name("acceptoffer"),
        make_tuple(
            sale_itr->offer_id
        )
    ).send();

    internal_transfer_assets(
        buyer,
        sale_itr->asset_ids,
        "AtomicMarket Purchased Sale - ID # " + to_string(sale_id)
    );

    sales.erase(sale_itr);
}


/**
* Checks whether the provided asset ids, listing price and settlement symbol match the values of
* the sale with the specified id and throws the transaction if this is not the case
* 
* Meant to be called within the same transaction as the purchase action for this sale in order to
* validate that the sale with the specified id contains what the purchaser expects it to contain
* 
* @required_auth None
*/
ACTION atomicmarket::assertsale(
    uint64_t sale_id,
    vector <uint64_t> asset_ids_to_assert,
    asset listing_price_to_assert,
    symbol settlement_symbol_to_assert
) {
    auto sale_itr = sales.require_find(sale_id,
        "No sale with this sale_id exists");
    
    check(asset_ids_to_assert == sale_itr->asset_ids,
        "The asset ids to assert differ from the asset ids of this sale");
    
    check(listing_price_to_assert == sale_itr->listing_price,
        "The listing price to assert differs from the listing price of this sale");
    
    check(settlement_symbol_to_assert == sale_itr->settlement_symbol,
        "The settlement symbol to assert differs from the settlement symbol of this sale");
}


/**
* Create an auction listing
* For the auction to become active, the seller needs to use the atomicassets transfer action to transfer the assets
* to the atomicmarket contract with the memo "auction"
* 
* duration is in seconds
* 
* @required_auth seller
*/
ACTION atomicmarket::announceauct(
    name seller,
    vector <uint64_t> asset_ids,
    asset starting_bid,
    uint32_t duration,
    name maker_marketplace
) {
    require_auth(seller);

    name assets_collection_name = get_collection_and_check_assets(seller, asset_ids);


    checksum256 asset_ids_hash = hash_asset_ids(asset_ids);

    auto auctions_by_hash = auctions.get_index <name("assetidshash")>();
    auto auction_itr = auctions_by_hash.find(asset_ids_hash);

    while (auction_itr != auctions_by_hash.end()) {
        if (asset_ids_hash != auction_itr->asset_ids_hash()) {
            break;
        }

        check(auction_itr->seller != seller,
            "You have already announced an auction for these assets. You can cancel an auction using the cancelauct action.");

        auction_itr++;
    }


    check(is_symbol_supported(starting_bid.symbol), "The specified starting bid token is not supported.");
    check(starting_bid.amount > 0, "The starting bid must be greater than zero");

    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

    double collection_fee = get_collection_fee(assets_collection_name);
    check(collection_fee <= atomicassets::MAX_MARKET_FEE,
        "The collection fee is too high. This should have been prevented by the atomicassets contract");

    config_s current_config = config.get();
    check(duration >= current_config.minimum_auction_duration,
        "The specified duration is shorter than the minimum auction duration");
    check(duration <= current_config.maximum_auction_duration,
        "The specified duration is longer than the maximum auction duration");

    uint64_t auction_id = consume_counter(name("auction"));

    auctions.emplace(seller, [&](auto &_auction) {
        _auction.auction_id = auction_id;
        _auction.seller = seller;
        _auction.asset_ids = asset_ids;
        _auction.end_time = current_time_point().sec_since_epoch() + duration;
        _auction.assets_transferred = false;
        _auction.current_bid = starting_bid;
        _auction.current_bidder = name("");
        _auction.claimed_by_seller = false;
        _auction.claimed_by_buyer = false;
        _auction.maker_marketplace = maker_marketplace;
        _auction.taker_marketplace = name("");
        _auction.collection_name = assets_collection_name;
        _auction.collection_fee = collection_fee;
    });


    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewauct"),
        make_tuple(
            auction_id,
            seller,
            asset_ids,
            starting_bid,
            duration,
            current_time_point().sec_since_epoch() + duration,
            maker_marketplace,
            assets_collection_name,
            collection_fee
        )
    ).send();
}


/**
* Cancels an auction. If the auction is active, it must not have any bids yet.
* Auctions with bids can't be cancelled.
* 
* If the auction is invalid (it is not active yet and the seller does not own at least one of the
* assets listed in the auction) this action can be called without the autorization of the seller
* 
* @required_auth seller
*/
ACTION atomicmarket::cancelauct(
    uint64_t auction_id
) {
    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");


    bool is_auction_invalid = false;

    if (!auction_itr->assets_transferred) {
        atomicassets::assets_t seller_assets = atomicassets::get_assets(auction_itr->seller);
        for (uint64_t asset_id : auction_itr->asset_ids) {
            if (seller_assets.find(asset_id) == seller_assets.end()) {
                is_auction_invalid = true;
                break;
            }
        }
    }

    check(is_auction_invalid || has_auth(auction_itr->seller),
        "The auction is not invalid, therefore the authorization of the seller is needed to cancel it");


    if (auction_itr->assets_transferred) {
        check(auction_itr->current_bidder == name(""),
            "This auction already has a bid. Auctions with bids can't be cancelled");

        internal_transfer_assets(
            auction_itr->seller,
            auction_itr->asset_ids,
            "AtomicMarket Cancelled Auction - ID # " + to_string(auction_id)
        );
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

    check(auction_itr->assets_transferred,
        "The auction is not yet active. The seller first needs to transfer the asset to the atomicmarket account");

    check(current_time_point().sec_since_epoch() < auction_itr->end_time,
        "The auction is already finished");

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
            auction_itr->current_bid
        );
    }

    internal_decrease_balance(
        bidder,
        bid
    );

    check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");

    auctions.modify(auction_itr, same_payer, [&](auto &_auction) {
        _auction.current_bid = bid;
        _auction.current_bidder = bidder;
        _auction.taker_marketplace = taker_marketplace;
        _auction.end_time = std::max(
            _auction.end_time,
            current_time_point().sec_since_epoch() + current_config.auction_reset_duration
        );
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

    check(auction_itr->assets_transferred, "The auction is not active");

    check(auction_itr->current_bidder != name(""),
        "The auction does not have any bids");

    require_auth(auction_itr->current_bidder);

    check(auction_itr->end_time < current_time_point().sec_since_epoch(),
        "The auction is not finished yet");
    
    check(!auction_itr->claimed_by_buyer,
        "The auction has already been claimed by the buyer");

    internal_transfer_assets(
        auction_itr->current_bidder,
        auction_itr->asset_ids,
        "AtomicMarket Won Auction - ID # " + to_string(auction_id)
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

    check(auction_itr->assets_transferred, "The auction is not active");

    check(auction_itr->end_time < current_time_point().sec_since_epoch(),
        "The auction is not finished yet");

    check(auction_itr->current_bidder != name(""),
        "The auction does not have any bids");
    
    check(!auction_itr->claimed_by_seller,
        "The auction has already been claimed by the seller");

    internal_payout_sale(
        auction_itr->current_bid,
        auction_itr->seller,
        auction_itr->maker_marketplace,
        auction_itr->taker_marketplace,
        get_collection_author(auction_itr->collection_name),
        auction_itr->collection_fee,
        "AtomicMarket Auction Payout - ID #" + to_string(auction_id)
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
* Checks whether the provided asset ids match those of the auction with the specified id
* and throws the transaction if this is not the case
* 
* Meant to be called within the same transaction as a bid action for this auction in order to
* validate that the auction with the specified id contains what the bidder expects it to contain
* 
* @required_auth None
*/
ACTION atomicmarket::assertauct(
    uint64_t auction_id,
    vector <uint64_t> asset_ids_to_assert
) {
    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this auction_id exists");
    
    check(asset_ids_to_assert == auction_itr->asset_ids,
        "The asset ids to assert differ from the asset ids of this auction");
}


/**
* Creates a buyoffer
* The specified price is deducted from the buyer's balance
* The recipient then has the option to trade the specified assets for the offered price (excluding fees)
* 
* @required_auth buyer
*/
ACTION atomicmarket::createbuyo(
    name buyer,
    name recipient,
    asset price,
    vector <uint64_t> asset_ids,
    string memo,
    name maker_marketplace
) {
    require_auth(buyer);

    check(buyer != recipient, "buyer and recipient can't be the same account");

    name assets_collection_name = get_collection_and_check_assets(recipient, asset_ids);

    // Not needed technically, as invalid symbols would simply fail when attempting to decrease
    // the balance. Only meant to give more meaningful error messages.
    check(is_symbol_supported(price.symbol), "The symbol of the specified price is not supported");

    check(price.amount > 0, "The price must be greater than zero");
    internal_decrease_balance(buyer, price);

    check(memo.length() <= 256, "A buyoffer memo can only be 256 characters max");


    check(is_valid_marketplace(maker_marketplace), "The maker marketplace is not a valid marketplace");

    uint64_t buyoffer_id = consume_counter(name("buyoffer"));

    buyoffers.emplace(buyer, [&](auto &_buyoffer) {
        _buyoffer.buyoffer_id = buyoffer_id;
        _buyoffer.buyer = buyer;
        _buyoffer.recipient = recipient;
        _buyoffer.price = price;
        _buyoffer.asset_ids = asset_ids;
        _buyoffer.memo = memo;
        _buyoffer.maker_marketplace = maker_marketplace;
        _buyoffer.collection_name = assets_collection_name;
        _buyoffer.collection_fee = get_collection_fee(assets_collection_name);
    });


    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewbuyo"),
        make_tuple(
            buyoffer_id,
            buyer,
            recipient,
            price,
            asset_ids,
            memo,
            maker_marketplace,
            assets_collection_name,
            get_collection_fee(assets_collection_name)
        )
    ).send();
}


/**
* Cancels (erases) a buyoffer
* The price that has previously been deducted when creating the buyoffer is added
* back to the buyer's balance
* 
* @required_auth The buyer of the buyoffer
*/
ACTION atomicmarket::cancelbuyo(
    uint64_t buyoffer_id
) {
    auto buyoffer_itr = buyoffers.require_find(buyoffer_id,
        "No buyoffer with this id exists");
    
    require_auth(buyoffer_itr->buyer);

    internal_add_balance(buyoffer_itr->buyer, buyoffer_itr->price);

    buyoffers.erase(buyoffer_itr);
}


/**
* Accepts a buyoffer
* Calling this action expects that the recipient of the buyoffer had created an AtomicAssets
* trade offer, which offers the assets of the buyoffer to the AtomicMarket contract, while
* asking for nothing in return and using the memo "buyoffer"
* 
* The AtomicAssets offer with the highest offer_id is looked at, which means that the recipient
* should create the AtomicAssets offer and then call this action within the same transaction to
* make sure that they are executed directly after one antoher
* 
* The AtomicMarket will then accept this trade offer and transfer the assets to the sender of
* the buyoffer, and pay out the offered price to the recipient
* 
* The price is subject to the same fees as sales or auctions
* 
* @required_auth The recipient of the buyoffer
*/
ACTION atomicmarket::acceptbuyo(
    uint64_t buyoffer_id,
    vector <uint64_t> expected_asset_ids,
    asset expected_price,
    name taker_marketplace
) {
    auto buyoffer_itr = buyoffers.require_find(buyoffer_id,
        "No buyoffer with this id exists");
    
    require_auth(buyoffer_itr->recipient);

    check(buyoffer_itr->asset_ids == expected_asset_ids,
        "The asset ids of this buyoffer differ from the expected asset ids");
    check(buyoffer_itr->price == expected_price,
        "The price of this buyoffer differ from the expected price");
    
    // This could theoretically fail if there is not a single AtomicAssets offer exists
    // Because it is assumed that this will rarely if ever be the case, no explicit check is added for that
    auto last_offer_itr = --atomicassets::offers.end();

    check(last_offer_itr->sender == buyoffer_itr->recipient && last_offer_itr->recipient == get_self(),
        "The last created AtomicAssets offer must be from the buyoffer recipient to the AtomicMarket contract");
    
    check(last_offer_itr->sender_asset_ids == buyoffer_itr->asset_ids,
        "The last created AtomicAssets offer must contain the assets of the buyoffer");
    check(last_offer_itr->recipient_asset_ids.size() == 0,
        "The last created AtomicAssets offer must not ask for any assets in return");
    
    check(last_offer_itr->memo == "buyoffer",
        "The last created AtomicAssets offer must have the memo \"buyoffer\"");


    // It is not checked whether the AtomicAssets offer is valid, because this will be checked in the
    // acceptoffer action, and if the offer is invalid, the transaction will throw
    action(
        permission_level{get_self(), name("active")},
        atomicassets::ATOMICASSETS_ACCOUNT,
        name("acceptoffer"),
        make_tuple(
            last_offer_itr->offer_id
        )
    ).send();

    internal_transfer_assets(
        buyoffer_itr->buyer,
        buyoffer_itr->asset_ids,
        "AtomicMarket Accepted Buyoffer - ID # " + to_string(buyoffer_id)
    );


    check(is_valid_marketplace(taker_marketplace), "The taker marketplace is not a valid marketplace");
    
    internal_payout_sale(
        buyoffer_itr->price,
        buyoffer_itr->recipient,
        buyoffer_itr->maker_marketplace,
        taker_marketplace,
        get_collection_author(buyoffer_itr->collection_name),
        buyoffer_itr->collection_fee,
        "AtomicMarket Buyoffer Payout - ID #" + to_string(buyoffer_id)
    );


    buyoffers.erase(buyoffer_itr);
}


/**
* Declines a buyoffer
* 
* @required_auth The recipient of the buyoffer
*/
ACTION atomicmarket::declinebuyo(
    uint64_t buyoffer_id,
    string decline_memo
) {
    auto buyoffer_itr = buyoffers.require_find(buyoffer_id,
        "No buyoffer with this id exists");
    
    require_auth(buyoffer_itr->recipient);

    check(decline_memo.length() <= 256, "A decline memo can only be 256 characters max");

    internal_add_balance(buyoffer_itr->buyer, buyoffer_itr->price);

    buyoffers.erase(buyoffer_itr);
}


/**
* Pays the RAM cost for an already existing sale
*/
ACTION atomicmarket::paysaleram(
    name payer,
    uint64_t sale_id
) {
    require_auth(payer);

    auto sale_itr = sales.require_find(sale_id,
        "No sale with this id exists");
    
    sales_s sale_copy = *sale_itr;

    sales.erase(sale_itr);

    sales.emplace(payer, [&](auto &_sale) {
        _sale = sale_copy;
    });
}


/**
* Pays the RAM cost for an already existing auction
*/
ACTION atomicmarket::payauctram(
    name payer,
    uint64_t auction_id
) {
    require_auth(payer);

    auto auction_itr = auctions.require_find(auction_id,
        "No auction with this id exists");
    
    auctions_s auction_copy = *auction_itr;

    auctions.erase(auction_itr);

    auctions.emplace(payer, [&](auto &_auction) {
        _auction = auction_copy;
    });
}


/**
* Pays the RAM cost for an already existing buyoffer
*/
ACTION atomicmarket::paybuyoram(
    name payer,
    uint64_t buyoffer_id
) {
    require_auth(payer);

    auto buyoffer_itr = buyoffers.require_find(buyoffer_id,
        "No buyoffer with this id exists");
    
    buyoffers_s buyoffer_copy = *buyoffer_itr;

    buyoffers.erase(buyoffer_itr);

    buyoffers.emplace(payer, [&](auto &_buyoffer) {
        _buyoffer = buyoffer_copy;
    });
}


/**
* This function is called when a transfer receipt from any token contract is sent to the atomicmarket contract
* It handels deposits and adds the transferred tokens to the sender's balance table row
*/
void atomicmarket::receive_token_transfer(name from, name to, asset quantity, string memo) {
    if (to != get_self()) {
        return;
    }

    check(is_token_supported(get_first_receiver(), quantity.symbol), "The transferred token is not supported");

    if (memo == "deposit") {
        internal_add_balance(from, quantity);
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
        checksum256 asset_ids_hash = hash_asset_ids(asset_ids);
        auto auctions_by_hash = auctions.get_index <name("assetidshash")>();
        auto auction_itr = auctions_by_hash.find(asset_ids_hash);

        while (true) {
            check(auction_itr != auctions_by_hash.end(),
                "No announced, non-finished auction by the sender for these assets exists");

            check(asset_ids_hash == auction_itr->asset_ids_hash(),
                "No announced, non-finished auction by the sender for these assets exists");

            if (auction_itr->seller == from && current_time_point().sec_since_epoch() < auction_itr->end_time) {
                break;
            }

            auction_itr++;
        }

        auctions_by_hash.modify(auction_itr, same_payer, [&](auto &_auction) {
            _auction.assets_transferred = true;
        });

        action(
            permission_level{get_self(), name("active")},
            get_self(),
            name("logauctstart"),
            make_tuple(
                auction_itr->auction_id
            )
        ).send();

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
        check(recipient_asset_ids.size() == 0, "You must not ask for any assets in return in a sale offer");


        checksum256 asset_ids_hash = hash_asset_ids(sender_asset_ids);

        auto sales_by_hash = sales.get_index <name("assetidshash")>();
        auto sale_itr = sales_by_hash.find(asset_ids_hash);

        while (true) {
            check(sale_itr != sales_by_hash.end(),
                "No sale was announced by this sender for the offered assets");

            check(asset_ids_hash == sale_itr->asset_ids_hash(),
                "No sale was announced by this sender for the offered assets");

            if (sale_itr->seller == sender) {
                break;
            }

            sale_itr++;
        }

        check(sale_itr->offer_id == -1, "An offer for this sale has already been created");

        sales_by_hash.modify(sale_itr, same_payer, [&](auto &_sale) {
            _sale.offer_id = offer_id;
        });

        action(
            permission_level{get_self(), name("active")},
            get_self(),
            name("logsalestart"),
            make_tuple(
                sale_itr->sale_id,
                offer_id
            )
        ).send();

    } else if (memo == "buyoffer") {
        // Offers for buyoffers are handled in the acceptbuyo action and require no immediate action
    } else {
        check(false, "Invalid memo");
    }
}


ACTION atomicmarket::lognewsale(
    uint64_t sale_id,
    name seller,
    vector <uint64_t> asset_ids,
    asset listing_price,
    symbol settlement_symbol,
    name maker_marketplace,
    name collection_name,
    double collection_fee
) {
    require_auth(get_self());

    require_recipient(seller);
}

ACTION atomicmarket::lognewauct(
    uint64_t auction_id,
    name seller,
    vector <uint64_t> asset_ids,
    asset starting_bid,
    uint32_t duration,
    uint32_t end_time,
    name maker_marketplace,
    name collection_name,
    double collection_fee
) {
    require_auth(get_self());

    require_recipient(seller);
}

ACTION atomicmarket::lognewbuyo(
    uint64_t buyoffer_id,
    name buyer,
    name recipient,
    asset price,
    vector <uint64_t> asset_ids,
    string memo,
    name maker_marketplace,
    name collection_name,
    double collection_fee
) {
    require_auth(get_self());
}

ACTION atomicmarket::logsalestart(
    uint64_t sale_id,
    uint64_t offer_id
) {
    require_auth(get_self());
}

ACTION atomicmarket::logauctstart(
    uint64_t auction_id
) {
    require_auth(get_self());
}


name atomicmarket::get_collection_and_check_assets(
    name owner,
    vector <uint64_t> asset_ids
) {
    check(asset_ids.size() != 0, "asset_ids needs to contain at least one id");

    vector <uint64_t> asset_ids_copy = asset_ids;
    std::sort(asset_ids_copy.begin(), asset_ids_copy.end());
    check(std::adjacent_find(asset_ids_copy.begin(), asset_ids_copy.end()) == asset_ids_copy.end(),
        "The asset_ids must not contain duplicates");


    atomicassets::assets_t owner_assets = atomicassets::get_assets(owner);

    name assets_collection_name = name("");
    for (uint64_t asset_id : asset_ids) {
        auto asset_itr = owner_assets.require_find(asset_id,
            ("The specified account does not own at least one of the assets - "
            + to_string(asset_id)).c_str());

        if (asset_itr->template_id != -1) {
            atomicassets::templates_t asset_template = atomicassets::get_templates(asset_itr->collection_name);
            auto template_itr = asset_template.find(asset_itr->template_id);
            check(template_itr->transferable,
                ("At least one of the assets is not transferable - " + to_string(asset_id)).c_str());
        }

        if (assets_collection_name == name("")) {
            assets_collection_name = asset_itr->collection_name;
        } else {
            check(assets_collection_name == asset_itr->collection_name,
                "The specified asset ids must all belong to the same collection");
        }
    }

    return assets_collection_name;
}


/**
* Gets the author of a collection in the atomicassets contract
*/
name atomicmarket::get_collection_author(name collection_name) {
    auto collection_itr = atomicassets::collections.find(collection_name.value);
    return collection_itr->author;
}


/**
* Gets the fee defined by a collection in the atomicassets contract
*/
double atomicmarket::get_collection_fee(name collection_name) {
    auto collection_itr = atomicassets::collections.find(collection_name.value);
    return collection_itr->market_fee;
}


/**
* Gets the current value of a counter and increments the counter by 1
* If no counter with the specified name exists yet, it is treated as if the counter was 1
*/
uint64_t atomicmarket::consume_counter(name counter_name) {
    uint64_t value;

    auto counter_itr = counters.find(counter_name.value);
    if (counter_itr == counters.end()) {
        value = 1; // Starting with 1 instead of 0 because these ids can be front facing
        counters.emplace(get_self(), [&](auto &_counter) {
            _counter.counter_name = counter_name;
            _counter.counter_value = 2;
        });
    } else {
        value = counter_itr->counter_value;
        counters.modify(counter_itr, get_self(), [&](auto &_counter) {
            _counter.counter_value++;
        });
    }
    
    return value;
}


/**
* Gets the token_contract corresponding to the token_symbol from the config
* Throws if there is no supported token with the specified token_symbol
*/
name atomicmarket::require_get_supported_token_contract(
    symbol token_symbol
) {
    config_s current_config = config.get();

    for (TOKEN supported_token : current_config.supported_tokens) {
        if (supported_token.token_symbol == token_symbol) {
            return supported_token.token_contract;
        }
    }

    check(false, "The specified token symbol is not supported");
    return name(""); //To silence the compiler warning
}


/**
* Gets the symbol pair with the provided listing and settlement symbol combination
* Throws if there is no symbol pair with the provided listing and settlement symbol combination
*/
atomicmarket::SYMBOLPAIR atomicmarket::require_get_symbol_pair(
    symbol listing_symbol,
    symbol settlement_symbol
) {
    config_s current_config = config.get();

    for (SYMBOLPAIR symbol_pair : current_config.supported_symbol_pairs) {
        if (symbol_pair.listing_symbol == listing_symbol && symbol_pair.settlement_symbol == settlement_symbol) {
            return symbol_pair;
        }
    }

    check(false, "No symbol pair with the specified listing - settlement symbol combination exists");
    return {}; //To silence the compiler warning
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
* Internal function to check whether a symbol pair with the specified listing and settlement symbols exists
*/
bool atomicmarket::is_symbol_pair_supported(
    symbol listing_symbol,
    symbol settlement_symbol
) {
    config_s current_config = config.get();

    for (SYMBOLPAIR symbol_pair : current_config.supported_symbol_pairs) {
        if (symbol_pair.listing_symbol == listing_symbol && symbol_pair.settlement_symbol == settlement_symbol) {
            return true;
        }
    }
    return false;
}


/**
* Checks if the provided marketplace is a valid marketplace
* A marketplace is valid if is in the marketplaces table
*/
bool atomicmarket::is_valid_marketplace(name marketplace) {
    return (marketplaces.find(marketplace.value) != marketplaces.end());
}


/**
* Decreases the withdrawers balance by the specified quantity and transfers the tokens to them
* Throws if the withdrawer does not have a sufficient balance
*/
void atomicmarket::internal_withdraw_tokens(
    name withdrawer,
    asset quantity,
    string memo
) {
    check(quantity.amount > 0, "The quantity to withdraw must be positive");

    //This will throw if the user does not have sufficient balance
    internal_decrease_balance(withdrawer, quantity);

    name withdraw_token_contract = require_get_supported_token_contract(quantity.symbol);

    action(
        permission_level{get_self(), name("active")},
        withdraw_token_contract,
        name("transfer"),
        make_tuple(
            get_self(),
            withdrawer,
            quantity,
            memo
        )
    ).send();
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
    double collection_fee,
    string seller_payout_message
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
        maker_itr->creator,
        asset(maker_cut_amount, quantity.symbol)
    );

    //Payout taker market
    auto taker_itr = marketplaces.find(taker_marketplace.value);
    internal_add_balance(
        taker_itr->creator,
        asset(taker_cut_amount, quantity.symbol)
    );

    //Payout collection
    internal_add_balance(
        collection_author,
        asset(collection_cut_amount, quantity.symbol)
    );

    //Payout seller
    internal_add_balance(
        seller,
        asset(seller_cut_amount, quantity.symbol)
    );

    internal_withdraw_tokens(seller, asset(seller_cut_amount, quantity.symbol), seller_payout_message);
}


/**
* Internal function used to add a quantity of a token to an account's balance
* It is not checked whether the added token is a supported token, this has to be checked before calling this function
*/
void atomicmarket::internal_add_balance(
    name owner,
    asset quantity
) {
    if (quantity.amount == 0) {
        return;
    }

    auto balance_itr = balances.find(owner.value);

    vector <asset> quantities;
    if (balance_itr == balances.end()) {
        //No balance table row exists yet
        quantities = {quantity};
        balances.emplace(get_self(), [&](auto &_balance) {
            _balance.owner = owner;
            _balance.quantities = quantities;
        });

    } else {
        //A balance table row already exists for owner
        quantities = balance_itr->quantities;

        bool found_token = false;
        for (asset &token : quantities) {
            if (token.symbol == quantity.symbol) {
                //If the owner already has a balance for the token, this balance is increased
                found_token = true;
                token.amount += quantity.amount;
                break;
            }
        }

        if (!found_token) {
            //If the owner does not already have a balance for the token, it is added to the vector
            quantities.push_back(quantity);
        }

        balances.modify(balance_itr, get_self(), [&](auto &_balance) {
            _balance.quantities = quantities;
        });
    }
}


/**
* Internal function used to deduct a quantity of a token from an account's balance
* If the account does not has less than that quantity in his balance, this function will cause the
* transaction to fail
*/
void atomicmarket::internal_decrease_balance(
    name owner,
    asset quantity
) {
    auto balance_itr = balances.require_find(owner.value,
        "The specified account does not have a balance table row");

    vector <asset> quantities = balance_itr->quantities;
    bool found_token = false;
    for (auto itr = quantities.begin(); itr != quantities.end(); itr++) {
        if (itr->symbol == quantity.symbol) {
            found_token = true;
            check(itr->amount >= quantity.amount,
                "The specified account's balance is lower than the specified quantity");
            itr->amount -= quantity.amount;
            if (itr->amount == 0) {
                quantities.erase(itr);
            }
            break;
        }
    }
    check(found_token,
        "The specified account does not have a balance for the symbol specified in the quantity");

    //Updating the balances table
    if (quantities.size() > 0) {
        balances.modify(balance_itr, same_payer, [&](auto &_balance) {
            _balance.quantities = quantities;
        });
    } else {
        balances.erase(balance_itr);
    }
}


void atomicmarket::internal_transfer_assets(
    name to,
    vector <uint64_t> asset_ids,
    string memo
) {
    action(
        permission_level{get_self(), name("active")},
        atomicassets::ATOMICASSETS_ACCOUNT,
        name("transfer"),
        make_tuple(
            get_self(),
            to,
            asset_ids,
            memo
        )
    ).send();
}