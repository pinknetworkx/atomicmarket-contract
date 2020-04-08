#ifndef ATOMICMARKET_HPP
#define ATOMICMARKET_HPP

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace std;
using namespace eosio;

static constexpr name ATOMICASSETS_ACCOUNT = name("atomicassets");
static constexpr name DEFAULT_MARKETPLACE_NAME = name("default");
static constexpr name DEFAULT_MARKETPLACE_FEE = name("pink.network");

CONTRACT atomicmarket : public contract {
  public:
    using contract::contract;

    ACTION init();
    ACTION setminbidinc(double minimum_bid_increase);
    ACTION addconftoken(name token_contract, symbol token_symbol);
    ACTION setmarketfee(double maker_market_fee, double taker_market_fee);

    ACTION regmarket(
      name fee_account,
      name marketplace_name
    );

    ACTION announcesale(
      name seller,
      uint64_t asset_id,
      asset price,
      name maker_marketplace
    );
    ACTION cancelsale(
      name seller,
      uint64_t asset_id
    );
    ACTION purchasesale(
      name buyer,
      name seller,
      uint64_t asset_id,
      asset intended_price,
      name taker_marketplace
    );

    ACTION announceauct(
      name seller,
      uint64_t asset_id,
      asset starting_bid,
      uint32_t duration,
      name maker_marketplace
    );
    ACTION cancelauct(
      name seller,
      uint64_t asset_id
    );
    ACTION auctionbid(
      name bidder,
      name seller,
      uint64_t asset_id,
      asset bid,
      name taker_marketplace
    );
    ACTION auctclaimbuy(
      name seller,
      uint64_t asset_id
    );
    ACTION auctclaimsel(
      name seller,
      uint64_t asset_id
    );

    void receive_token_transfer(
      name from,
      name to,
      asset quantity,
      string memo
    );
    void receive_asset_transfer(
      name from,
      name to,
      vector<uint64_t> asset_ids,
      string memo
    );
    void receive_asset_offer(
      uint64_t offer_id,
      name sender,
      name recipient,
      vector<uint64_t> sender_asset_ids,
      vector<uint64_t> recipient_asset_ids,
      string memo
    );


    ACTION logincbal(
      name user,
      asset balance_increase,
      string reason
    );
    ACTION logdecbal(
      name user,
      asset balance_decrease,
      string reason
    );

  private:
    struct TOKEN {
      name token_contract;
      symbol token_symbol;
    };


    TABLE balances_s {
      name                  owner;
      vector<asset>         quantities;

      uint64_t primary_key() const { return owner.value; };
    };
    typedef multi_index<name("balances"), balances_s> balances_t;


    //Scope: seller
    TABLE sales_s {
      uint64_t              asset_id;
      asset                 price;
      int64_t               offer_id;   //-1 if no offer has been created yet, else the offer id
      name                  maker_marketplace;
      double                collection_fee;

      uint64_t primary_key() const { return asset_id; };
    };
    typedef multi_index<name("sales"), sales_s> sales_t;


    //Scope: seller
    TABLE auctions_s {
      uint64_t              asset_id;
      uint32_t              end_time;   //seconds since epoch
      asset                 current_bid;
      name                  current_bidder;
      bool                  is_active;
      bool                  claimed_by_seller;
      bool                  claimed_by_buyer;
      name                  maker_marketplace;
      name                  taker_marketplace;
      double                collection_fee;
      name                  collection_author;

      uint64_t primary_key() const { return asset_id; };
    };
    typedef multi_index<name("auctions"), auctions_s> auctions_t;

    TABLE marketplaces_s {
      name                  marketplace_name;
      name                  fee_account;

      uint64_t primary_key() const { return marketplace_name.value; };
    };
    typedef multi_index<name("marketplaces"), marketplaces_s> marketplaces_t;


    TABLE config_s {
      double              minimum_bid_increase = 0.1;
      uint32_t            maximum_auction_duration = 2592000; //30 days
      vector<TOKEN>       supported_tokens = {};
      double              maker_market_fee = 0.01;
      double              taker_market_fee = 0.01;
    };
    typedef singleton<name("config"), config_s> config_t;
    // https://github.com/EOSIO/eosio.cdt/issues/280
    typedef multi_index<name("config"), config_s> config_t_for_abi;


    //The following tables only exists in the atomicassets contract, not the atomicmarket contract

    TABLE collections_s {
      name                collection_name;
      name                author;
      bool                allow_notify;
      vector<name>        authorized_accounts;
      vector<name>        notify_accounts;
      double              market_fee;
      vector<uint8_t>     serialized_data;

      uint64_t primary_key() const { return collection_name.value; };
    };
    typedef multi_index<name("collections"), collections_s> collections_t;

    //Scope: owner
    TABLE assets_s {
      uint64_t            asset_id;
      name                collection_name;
      name                scheme_name;
      int32_t             preset_id;
      name                ram_payer;
      vector<asset>       backed_tokens;
      vector<uint8_t>     immutable_serialized_data;
      vector<uint8_t>     mutable_serialized_data;

      uint64_t primary_key() const { return asset_id; };
    };
    typedef multi_index<name("assets"), assets_s> atomicassets_assets_t;


    TABLE offers_s {
      uint64_t            offer_id;
      name                offer_sender;
      name                offer_recipient;
      vector<uint64_t>    sender_asset_ids;
      vector<uint64_t>    recipient_asset_ids;
      string              memo;

      uint64_t primary_key() const { return offer_id; };
      uint64_t by_sender() const { return offer_sender.value; };
      uint64_t by_recipient() const { return offer_recipient.value; };
    };
    typedef multi_index<name("offers"), offers_s,
    indexed_by<name("sender"), const_mem_fun<offers_s, uint64_t, &offers_s::by_sender>>,
    indexed_by<name("recipient"), const_mem_fun<offers_s, uint64_t, &offers_s::by_recipient>>> offers_t;


    balances_t balances = balances_t(get_self(), get_self().value);
    marketplaces_t marketplaces = marketplaces_t(get_self(), get_self().value);
    config_t config = config_t(get_self(), get_self().value);

    offers_t atomicassets_offers = offers_t(ATOMICASSETS_ACCOUNT, ATOMICASSETS_ACCOUNT.value);
    collections_t atomicassets_collections = collections_t(ATOMICASSETS_ACCOUNT, ATOMICASSETS_ACCOUNT.value);


    bool is_token_supported(name token_contract, symbol token_symbol);
    bool is_symbol_supported(symbol token_symbol);
    bool is_valid_marketplace(name marketplace);

    name get_collection_author(uint64_t asset_id, name asset_owner);
    double get_collection_fee(uint64_t asset_id, name asset_owner);

    void internal_payout_sale(
      asset quantity,
      name seller,
      name maker_marketplace,
      name taker_marketplace,
      name collection_author,
      double collection_fee
    );

    void internal_add_balance(name user, asset quantity, string reason);
    void internal_deduct_balance(name user, asset quantity, string reason);
    void internal_transfer_asset(name to, uint64_t asset_id, string memo);


    sales_t get_sales(name seller);
    auctions_t get_auctions(name seller);

    atomicassets_assets_t get_atomicassets_assets(name acc);
};


extern "C"
void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
	if (code == receiver) {
		switch (action) {
      EOSIO_DISPATCH_HELPER(atomicmarket, \
      (init)(setminbidinc)(addconftoken)(setmarketfee)(regmarket) \
      (announcesale)(cancelsale)(purchasesale) \
      (announceauct)(cancelauct)(auctionbid)(auctclaimbuy)(auctclaimsel) \
      (logincbal)(logdecbal))
		}
	} else if (code == ATOMICASSETS_ACCOUNT.value && action == name("transfer").value) {
    eosio::execute_action(name(receiver), name(code), &atomicmarket::receive_asset_transfer);

  } else if (code == ATOMICASSETS_ACCOUNT.value && action == name("lognewoffer").value) {
    eosio::execute_action(name(receiver), name(code), &atomicmarket::receive_asset_offer);

  } else if (action == name("transfer").value) {
    eosio::execute_action(name(receiver), name(code), &atomicmarket::receive_token_transfer);
  }
}

#endif