# AtomicMarket
AtomicMarket is a marketplace to sell and auction [AtomicAssets](https://github.com/pinknetworkx/atomicassets-contract) NFTs.

### [Documentation can be found here.](https://github.com/pinknetworkx/atomicmarket-contract/wiki)

## Useful links
- API: https://github.com/pinknetworkx/eosio-contract-api
- Live API example: https://wax.api.atomicassets.io/atomicmarket/docs/
- Javascript module: https://www.npmjs.com/package/atomicmarket
- Test cases (using Hydra framework): https://github.com/pinknetworkx/atomicmarket-contract-tests
- Telegram group: https://t.me/atomicassets

## Key Features
	
- **NFTs do not have to be transferred for sales**

	Instead of using transfers, AtomicAssets **offers** are used for sales. These offers are only accepted when someone buys the NFTs for sale. Therefore, sellers keep ownership over their NFTs while they are listed on the AtomicMarket.

- **Selling / Auctioning multiple NFTs at once**

	Multiple NFTs can be sold / auctioned as bundles. The only requirement for this is that they belong to the same collection.

- **Support for any standard token**

	The AtomicMarket supports adding any number of standard tokens to support. This means that the market is not limited to a chain's core token (e.g. WAX), but instead new tokens can be added, which can then be used to sell and auction NFTs for.

- **Delphioracle Sales**

	On top of normal sales for standard tokens, the AtomicMarket contract can also use the [delphioracle](https://github.com/eostitan/delphioracle) to allow selling assets for any symbol which is then converted into an on-chain token at the time of purchase. An example would be listing an NFT for USD, which is then paid in WAX tokens at the exchange rate at the time of the pruchase.
	
- **Decentralized fee structure**

	There is no central account that receives fees for sales / auctions. Instead, anyone can register as a marketplace and receive fees for the sales / auctions they facilitate. By default, the maker marketplace (facilitating the listing) and the taker marketplace (facilitating the purchase) both receive **1% each** of any sale / auction.
	
- **Collection fees**

	Collections can define a market fee between 0 and 15% in the AtomicAssets contract. This fee is repected by the AtomicMarket and paid to the authors of the collection.
