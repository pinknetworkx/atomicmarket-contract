#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace std;
using namespace eosio;

static constexpr name ATOMICASSETS_ACCOUNT = name("atomicassets");
static constexpr name DELPHIORACLE_ACCOUNT = name("delphioracle");
static constexpr name DEFAULT_MARKETPLACE_NAME = name("default");
static constexpr name DEFAULT_MARKETPLACE_FEE = name("pink.network");

CONTRACT atomicmarket : public contract {
public:
    using contract::contract;

    ACTION init();

    ACTION setminbidinc(double minimum_bid_increase);

    ACTION addconftoken(name token_contract, symbol token_symbol);

    ACTION adddelphi(name delphi_pair_name, symbol stable_symbol, symbol token_symbol);

    ACTION setmarketfee(double maker_market_fee, double taker_market_fee);


    ACTION regmarket(
        name fee_account,
        name marketplace_name
    );


    ACTION withdraw(
        name from,
        asset quantity
    );


    ACTION announcesale(
        name seller,
        uint64_t asset_id,
        asset price,
        name maker_marketplace
    );

    ACTION cancelsale(
        uint64_t sale_id
    );

    ACTION purchasesale(
        name buyer,
        uint64_t sale_id,
        name taker_marketplace
    );

    ACTION announcestbl(
        name seller,
        uint64_t asset_id,
        name delphi_pair_name,
        asset price,
        name maker_marketplace
    );

    ACTION cancelstbl(
        uint64_t stable_sale_id
    );

    ACTION purchasestbl(
        name buyer,
        uint64_t stable_sale_id,
        uint64_t intended_delphi_median,
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
        uint64_t auction_id
    );

    ACTION auctionbid(
        name bidder,
        uint64_t auction_id,
        asset bid,
        name taker_marketplace
    );

    ACTION auctclaimbuy(
        uint64_t auction_id
    );

    ACTION auctclaimsel(
        uint64_t auction_id
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
        vector <uint64_t> asset_ids,
        string memo
    );

    void receive_asset_offer(
        uint64_t offer_id,
        name sender,
        name recipient,
        vector <uint64_t> sender_asset_ids,
        vector <uint64_t> recipient_asset_ids,
        string memo
    );

    ACTION lognewsale(
        uint64_t sale_id,
        name seller,
        uint64_t asset_id,
        asset price,
        name maker_marketplace
    );

    ACTION lognewstbl(
        uint64_t stable_sale_id,
        name seller,
        uint64_t asset_id,
        name delphi_pair_name,
        asset price,
        name maker_marketplace
    );

    ACTION lognewauct(
        uint64_t auction_id,
        name seller,
        uint64_t asset_id,
        asset starting_bid,
        uint32_t duration,
        name maker_marketplace
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

    struct DELPHIPAIR {
        name delphi_pair_name;
        symbol stable_symbol;
        symbol token_symbol;
    };


    TABLE balances_s{
        name                    owner;
        vector<asset>           quantities;

        uint64_t primary_key() const { return owner.value; };
    };
    typedef multi_index<name("balances"), balances_s> balances_t;


    TABLE sales_s{
        uint64_t                sale_id;
        name                    seller;
        uint64_t                asset_id;
        asset                   price;
        int64_t                 offer_id;   //-1 if no offer has been created yet, else the offer id
        name                    maker_marketplace;
        double                  collection_fee;

        uint64_t primary_key() const { return sale_id; };
        uint64_t by_assetid() const { return asset_id; };
    };
    typedef multi_index<name("sales"), sales_s,
        indexed_by < name("assetid"), const_mem_fun < sales_s, uint64_t, &sales_s::by_assetid>>>
    sales_t;


    TABLE stable_sales_s{
        uint64_t                stable_sale_id;
        name                    seller;
        uint64_t                asset_id;
        name                    delphi_pair_name;
        asset                   price;
        int64_t                 offer_id;   //-1 if no offer has been created yet, else the offer id
        name                    maker_marketplace;
        double                  collection_fee;

        uint64_t primary_key() const { return stable_sale_id; };
        uint64_t by_assetid() const { return asset_id; };
    };
    typedef multi_index<name("stablesales"), stable_sales_s,
        indexed_by < name("assetid"), const_mem_fun < stable_sales_s, uint64_t, &stable_sales_s::by_assetid>>>
    stable_sales_t;


    TABLE auctions_s{
        uint64_t                auction_id;
        name                    seller;
        uint64_t                asset_id;
        uint32_t                end_time;   //seconds since epoch
        bool                    is_active;
        asset                   current_bid;
        name                    current_bidder;
        bool                    claimed_by_seller;
        bool                    claimed_by_buyer;
        name                    maker_marketplace;
        name                    taker_marketplace;
        double                  collection_fee;
        name                    collection_author;

        uint64_t primary_key() const { return auction_id; };
        uint64_t by_assetid() const { return asset_id; };
    };
    typedef multi_index<name("auctions"), auctions_s,
        indexed_by < name("assetid"), const_mem_fun < auctions_s, uint64_t, &auctions_s::by_assetid>>>
    auctions_t;


    TABLE marketplaces_s{
        name                    marketplace_name;
        name                    fee_account;

        uint64_t primary_key() const { return marketplace_name.value; };
    };
    typedef multi_index<name("marketplaces"), marketplaces_s> marketplaces_t;


    TABLE config_s{
        uint64_t                sale_counter = 1;
        uint64_t                stable_sale_counter = 1;
        uint64_t                auction_counter = 1;
        double                  minimum_bid_increase = 0.1;
        uint32_t                maximum_auction_duration = 2592000; //30 days
        vector<TOKEN>           supported_tokens = {};
        vector<DELPHIPAIR>      supported_delphi_pairs = {};
        double                  maker_market_fee = 0.01;
        double                  taker_market_fee = 0.01;
    };
    typedef singleton<name("config"), config_s> config_t;
    // https://github.com/EOSIO/eosio.cdt/issues/280
    typedef multi_index<name("config"), config_s> config_t_for_abi;


    sales_t sales = sales_t(get_self(), get_self().value);
    stable_sales_t stable_sales = stable_sales_t(get_self(), get_self().value);
    auctions_t auctions = auctions_t(get_self(), get_self().value);
    balances_t balances = balances_t(get_self(), get_self().value);
    marketplaces_t marketplaces = marketplaces_t(get_self(), get_self().value);
    config_t config = config_t(get_self(), get_self().value);


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

};


extern "C"
void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(atomicmarket, \
            (init)(setminbidinc)(addconftoken)(adddelphi)(setmarketfee)(regmarket)(withdraw) \
            (announcesale)(cancelsale)(purchasesale) \
            (announcestbl)(cancelstbl)(purchasestbl) \
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