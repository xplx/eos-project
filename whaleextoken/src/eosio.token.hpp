/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
class system_contract;
}

namespace eosio {

using std::string;

class token: public contract {
public:
	token(name self) :
			contract(self) {
	}

	void create(name issuer, asset maximum_supply);

	void issue(name to, asset quantity, string memo);

	void retire(asset quantity, string memo);

	void transfer(name from, name to, asset quantity, string memo);

	void close(name owner, symbol_type symbol);

	void addblacklist(name account, symbol_type symbol);

	void rmblacklist(name account, symbol_type symbol);

	inline asset get_supply(symbol_name sym) const;

	inline asset get_balance(name owner, symbol_name sym) const;

private:
	struct account {
		asset balance;

		uint64_t primary_key() const {
			return balance.symbol.name();
		}
	};

	struct currency_stats {
		asset supply;
		asset max_supply;
		name issuer;

		uint64_t primary_key() const {
			return supply.symbol.name();
		}
	};

	struct blacklist {
		name account;

		uint64_t primary_key() const {
			return account;
		}
	};

	typedef eosio::multi_index<N(accounts), account> accounts;
	typedef eosio::multi_index<N(stat), currency_stats> stats;
	typedef eosio::multi_index<N(blacklist), blacklist> blacklists;

	void sub_balance(name owner, asset value);
	void add_balance(name owner, asset value, name ram_payer);

public:
	struct transfer_args {
		name from;
		name to;
		asset quantity;
		string memo;
	};
};

asset token::get_supply(symbol_name sym) const {
	stats statstable(_self, sym);
	const auto& st = statstable.get(sym);
	return st.supply;
}

asset token::get_balance(name owner, symbol_name sym) const {
	accounts accountstable(_self, owner);
	const auto& ac = accountstable.get(sym);
	return ac.balance;
}

} /// namespace eosio
