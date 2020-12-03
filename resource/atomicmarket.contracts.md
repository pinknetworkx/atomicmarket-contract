<h1 class="contract">init</h1>

---
spec_version: "0.2.0"
title: Initialize config table
summary: 'Initialize the table "config" if it has not been initialized before'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
Initialize the table "config" if it has not been initialized before. If it has been initialized before, nothing will happen.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">setminbidinc</h1>

---
spec_version: "0.2.0"
title: Set minimum auction bid increase
summary: 'Sets the minimum auction bid increase to {{nowrap minimum_bid_increase}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---
<b>Description:</b>
<div class="description">
The minimum bid increase for auctions to {{minimum_bid_increase}}.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">setversion</h1>

---
spec_version: "0.2.0"
title: Set config version
summary: 'Sets the version in the config table to {{nowrap new_version}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---
<b>Description:</b>
<div class="description">
The version in the config table is set to {{new_version}}.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">addconftoken</h1>

---
spec_version: "0.2.0"
title: Add token to supported list
summary: 'Adds a to the supported tokens list in the config'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---
<b>Description:</b>
<div class="description">
The token with the symbol {{token_symbol}} from the token contract {{token_contract}} is added to the supported_tokens list.

This means this token can then be deposited and used for sales and auctions.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">adddelphi</h1>

---
spec_version: "0.2.0"
title: Add a delphi symbol pair
summary: 'Adds a pair to the supported symbol pairs list in the config'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---
<b>Description:</b>
<div class="description">
A new symbol pair is added to the config.

It allows users to list an asset for sale with a listing price specified in {{listing_symbol}} which will be paid (settled) in {{settlement_symbol}}, which belongs to a supported token.

The exchange rate is calculated using the {{delphi_pair_name}} delphioracle pair.

{{#if invert_delphi_pair}}The delphioracle price data will be inverted.
{{/if}}
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">setmarketfee</h1>

---
spec_version: "0.2.0"
title: Set the market fees
summary: 'Sets the market fees that are paid out to the marketplaces facilitating sales and auctions'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---
<b>Description:</b>
<div class="description">
The share that the maker market place will receive of successful sales and auctions is set to {{maker_market_fee}}.

The share that the taker market place will receive of successful sales and auctions is set to {{taker_market_fee}}.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{$action.account}}.
</div>




<h1 class="contract">regmarket</h1>

---
spec_version: "0.2.0"
title: Register a new market
summary: '{{nowrap creator}} creates a new marketplace with the name {{nowrap marketplace_name}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{nowrap creator}} creates a new marketplace with the name {{nowrap marketplace_name}}.

This marketplace name can then be used in the "announcesale", "announceauct", "purchasesale", "auctionbid" actions.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{creator}}.
</div>




<h1 class="contract">withdraw</h1>

---
spec_version: "0.2.0"
title: Withdraw fungible tokens
summary: '{{nowrap owner}} withdraws {{token_to_withdraw}} from his balance'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{owner}} withdraws {{token_to_withdraw}}.
The tokens will be transferred back to {{owner}} and will be deducted from {{owner}}'s balance.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{owner}}.
</div>




<h1 class="contract">announcesale</h1>

---
spec_version: "0.2.0"
title: Announce a sale
summary: '{{nowrap seller}} announces a sale of one or multiple assets'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{seller}} announces a sale for the assets with the following IDs, which all have to belong to the same AtomicAssets collection:
{{#each asset_ids}}
    - {{this}}
{{/each}}

For this sale to become active, {{seller}} has to create an AtomicAssets trade offer in which he offers the aforementioned assets to the AtomicMarket account with the memo "sale".

The assets will be listed for the price of {{listing_price}} which will be settled in {{symbol_to_symbol_code settlement_symbol}}.

{{#if maker_marketplace}}The marketplace with the name {{maker_marketplace}} facilitates this listing.
{{else}}The default marketplace facilitates this listing.
{{/if}}

If the sale is purchased, the marketplace facilitating the listing of the sale and the marketplace facilitating the purchase of the sale each receive a share of the sale price.

If the sale is purchased, the author of the collection that the listed assets belong to receives a share of the sale price.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{seller}}.
</div>




<h1 class="contract">cancelsale</h1>

---
spec_version: "0.2.0"
title: Cancel a sale
summary: 'The sale with the ID {{nowrap sale_id}} is cancelled'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The sale with the ID {{sale_id}} is cancelled.

If the seller of this sale has created an AtomicAssets trade offer, offering the assets for this sale to the AtomicMarket account, this trade offer will be declined.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the seller of the sale with the ID {{sale_id}}.
</div>




<h1 class="contract">purchasesale</h1>

---
spec_version: "0.2.0"
title: Purchase a sale
summary: '{{nowrap buyer}} purchases the sale with the ID {{nowrap sale_id}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{buyer}} purchases the sale with the ID {{sale_id}}.

If the sale's listing price uses a different symbol than the sale's settlement symbol, the following delphioracle median price is used to calculate the exchange rate: {{intended_delphi_median}}.

This delphioracle median price must have been reported to the delphioracle and must still be present in the delphioracle's datapoints table for the relevant delphi pair.

{{buyer}} will be transferred the assets of the sale.

The price of the sale will be deducted from {{buyer}}'s balance.

The marketplaces facilitating the sale listing and the purchase, the author of the collection that the sold assets belong to, and the seller each get their share of the sale price added to their balances.

{{#if taker_marketplace}}The marketplace with the name {{taker_marketplace}} facilitates this purchase.
{{else}}The default marketplace facilitates this purchase.
{{/if}}
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{buyer}}.
</div>




<h1 class="contract">assertsale</h1>

---
spec_version: "0.2.0"
title: Asserts sale details
summary: 'The asset ids and price of the sale {{nowrap sale_id}} is asserted'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
Asserts whether the sale with the id {{sale_id}} is for the asset ids {{asset_ids_to_assert}}, whether the listing price is {{listing_price_to_assert}} and whether the settlement symbol is {{settlement_symbol_to_assert}}
If any of these are not true, the transaction fails. Otherwise, nothing happens.
</div>

<b>Clauses:</b>
<div class="clauses">
</div>




<h1 class="contract">announceauct</h1>

---
spec_version: "0.2.0"
title: Announce an auction
summary: '{{nowrap seller}} announces an auction of one or multiple assets'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{seller}} announces an auction for the assets with the following IDs, which all have to belong to the same AtomicAssets collection:
{{#each asset_ids}}
    - {{this}}
{{/each}}

For this auction to become active, {{seller}} has to transfer the aforementioned assets to the AtomicMarket account with the memo "auction".

The starting bid for this auction will be {{starting_bid}} and the auction will run for a minimum of {{duration}} seconds, starting from the time of announcement.

{{#if maker_marketplace}}The marketplace with the name {{maker_marketplace}} facilitates this auction creation.
{{else}}The default marketplace facilitates this auction creation.
{{/if}}

If the auciton is successful, the marketplace facilitating the creation of the auction and the marketplace facilitating the final bid on the auction each receive a share of the final bid.

If the auciton is successful, the author of the collection that the listed assets belong to receives a share of the sale final bid.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{seller}}.
</div>




<h1 class="contract">cancelauct</h1>

---
spec_version: "0.2.0"
title: Cancel an auction
summary: 'The auction with the ID {{nowrap auction_id}} is cancelled'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The auction with the ID {{auction_id}} is cancelled. The auction must not have any bids yet, otherwise it can't be cancelled.

If the seller of this auction has already transferred the assets for this auction to the AtomicMarket account, the assets are transferred back.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the seller of the auction with the ID {{sale_id}}.
</div>




<h1 class="contract">auctionbid</h1>

---
spec_version: "0.2.0"
title: Place a bid on an auction
summary: '{{nowrap bidder}} bids {{nowrap bid}} on the auction with the ID {{nowrap sale_id}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{bidder}} places a bid of {{bid}} on the auction with the ID {{auction_id}}.

If the auction does not have any previous bids, the placed bid must be at least as high as the specified minimum bid of the auction.

If the auction does have a previous bid, the minimum relative increase of the bid is specified in the config table in the field minimum_bid_increase.

The price of the sale will be deducted from {{buyer}}'s balance.

If the auction has a previous bid, the previous bidder is refunded their bid into their balance.

{{#if taker_marketplace}}The marketplace with the name {{taker_marketplace}} facilitates this bid.
{{else}}The default marketplace facilitates this bid.
{{/if}}
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{bidder}}.
</div>




<h1 class="contract">auctclaimbuy</h1>

---
spec_version: "0.2.0"
title: Claim an auction as the buyer
summary: 'The highest bidder of the finished auction with the ID {{nowrap auction_id}} claims the assets won'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The winner (highest bidder) of the auction with the ID {{auction_id}} claims the assets won in the auction. The auction must be finished.

The assets won in the auction are transferred to the auction's winner.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the winner of the auction with the ID {{auction_id}}.
</div>




<h1 class="contract">auctclaimsel</h1>

---
spec_version: "0.2.0"
title: Claim an auction as the seller
summary: 'The seller of the finished auction with the ID {{nowrap auction_id}} claims the final bid'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The seller of the auction with the ID {{auction_id}} claims the assets won in the auction. The auction must be finished.

The marketplaces facilitating the auction creation and the final bid, the author of the collection that the auctioned assets belong to, and the seller each get their share of the final bid added to their balances.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the seller of the auction with the ID {{auction_id}}.
</div>




<h1 class="contract">assertauct</h1>

---
spec_version: "0.2.0"
title: Asserts auction details
summary: 'The asset ids of the auction {{nowrap auction_id}} is asserted'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
Asserts whether the auction with the id {{auction_id}} is for the asset ids {{asset_ids_to_assert}}
If it is not, the transaction fails. Otherwise, nothing happens.
</div>

<b>Clauses:</b>
<div class="clauses">
</div>




<h1 class="contract">createbuyo</h1>

---
spec_version: "0.2.0"
title: Create a buyoffer
summary: '{{nowrap sender}} creates a buyoffer for {{nowrap recipient}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{sender}} creates a buyoffer, offering {{price}} for the assets with the following ids, owned by {{recipient}}:
{{#each asset_ids}}
    - {{this}}
{{/each}}

The price is deducted from {{sender}}'s balance.

{{recipient}} may accept this buyoffer, exchanging the previously mentioned assets for the specified price (excluding fees).

{{#if maker_marketplace}}The marketplace with the name {{maker_marketplace}} facilitates this buyoffer creation.
{{else}}The default marketplace facilitates this buyoffer creation.
{{/if}}

{{#if memo}}There is a memo attached to the buyoffer stating:
    {{memo}}
{{else}}No memo is attached to the buyoffer.
{{/if}}
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{sender}}.
</div>




<h1 class="contract">cancelbuyo</h1>

---
spec_version: "0.2.0"
title: Cancels a buyoffer
summary: 'The buyoffer {{nowrap buyoffer_id}} is cancelled'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The buyoffer with the id {{buyoffer_id}} is cancelled.

The price of the buyoffer is added to the balance of the buyoffer's sender.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the sender of the buyoffer.
</div>




<h1 class="contract">acceptbuyo</h1>

---
spec_version: "0.2.0"
title: Accepts a buyoffer
summary: 'The buyoffer {{nowrap buyoffer_id}} is accepted'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The buyoffer with the id {{buyoffer_id}} is accepted by the recipient of the buyoffer.

If the asset ids of the buyoffer differ from {{expected_asset_ids}}, the transaction fails.

If the price of the buyoffer differs from {{expected_price}}, the transaction fails.

{{#if taker_marketplace}}The marketplace with the name {{taker_marketplace}} facilitates this buyoffer acceptance.
{{else}}The default marketplace facilitates this buyoffer acceptance.
{{/if}}

The recipient needs to have previously created an AtomicAssets trade offer, offerring the assets of the buyoffer to the AtomicMarket account without asking for anything in return.

The AtomicAssets trade offer is accepted and the assets of the buyoffer are forwarded to the sender of the buyoffer.

The marketplaces facilitating the buyoffer creation and the acceptance, the author of the collection that the traded assets belong to, and the recipient of the buyoffer each get their share of the offered price added to their balances.

</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the recipient of the buyoffer.
</div>




<h1 class="contract">declinebuyo</h1>

---
spec_version: "0.2.0"
title: Declines a buyoffer
summary: 'The buyoffer {{nowrap buyoffer_id}} is declined'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
The buyoffer with the id {{buyoffer_id}} is declined by the recipient of the buyoffer.

The price of the buyoffer is added to the balance of the buyoffer's sender.

{{#if decline_memo}}There is a memo attached to the decline stating:
    {{decline_memo}}
{{else}}No memo is attached to the decline.
{{/if}}

</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of the recipient of the buyoffer.
</div>




<h1 class="contract">paysaleram</h1>

---
spec_version: "0.2.0"
title: Pay for the RAM of a sale 
summary: '{{nowrap payer}} pays for the RAM of the sale with the ID {{nowrap sale_id}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{payer}} pays for the RAM associated with the table entry of the sale with the ID {{sale_id}}. The content of the table entry does not change.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{payer}}.
</div>




<h1 class="contract">payauctram</h1>

---
spec_version: "0.2.0"
title: Pay for the RAM of an auction 
summary: '{{nowrap payer}} pays for the RAM of the auction with the ID {{nowrap auction_id}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{payer}} pays for the RAM associated with the table entry of the auction with the ID {{auction_id}}. The content of the table entry does not change.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{payer}}.
</div>




<h1 class="contract">paybuyoram</h1>

---
spec_version: "0.2.0"
title: Pay for the RAM of a buyoffer 
summary: '{{nowrap payer}} pays for the RAM of the buyoffer with the ID {{nowrap buyoffer_id}}'
icon: https://atomicassets.io/image/logo256.png#108AEE3530F4EB368A4B0C28800894CFBABF46534F48345BF6453090554C52D5
---

<b>Description:</b>
<div class="description">
{{payer}} pays for the RAM associated with the table entry of the buyoffer with the ID {{buyoffer_id}}. The content of the table entry does not change.
</div>

<b>Clauses:</b>
<div class="clauses">
This action may only be called with the permission of {{payer}}.
</div>