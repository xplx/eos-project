/**
 * This file is part of eosioeightam smart contract, which is published at
 * https://github.com/8amfund/eosioeightam. See LICENSE file in the project root
 * for full license information.
 */
#include "eosioeightam.hpp"

const uint64_t SECOND = 1000 * 1000;
const uint64_t HOUR = SECOND * 60 * 60;
const uint64_t DAY = HOUR * 24;

void FundContract::addfund(uint64_t fund_id, std::string fund_name, std::string manager_name,
							std::string raising_address,eosio::name fund_admin,
							uint64_t start_time, uint64_t end_time, eosio::asset fund_size
) {
	//Ê¹ÓÃ»õ±Ò·ûºÅ
    using eosio::symbol_code;
    using eosio::symbol;

    require_auth(_self);
    uint64_t now = publication_time();
    eosio_assert(fund_name.length() <= 256, "length of fund_name should not exceed 256");
    eosio_assert(manager_name.length() <= 256, "length of manager_name should not exceed 256");
    eosio_assert(raising_address.length() <= 256, "length of raising_address should not exceed 256");
    eosio_assert(start_time < end_time, "start_time must be before end_time");
    eosio_assert(now < end_time, "cannot add ended fund");
    eosio_assert(fund_size.amount >= 0, "fund_size should not be negative");
    eosio_assert(fund_size.is_valid(), "fund_size is invalid");
    eosio_assert(
        fund_size.symbol == symbol(symbol_code("BTC"), 8) ||
        fund_size.symbol == symbol(symbol_code("USDT"), 8) ||
        fund_size.symbol == symbol(symbol_code("ETH"), 9) || // Gwei precision
        fund_size.symbol == symbol(symbol_code("EOS"), 4),
        "unknown fund_size symbol"
    );

    FundTable funds(_self, _self.value);
    funds.emplace(_self, [&](Fund& item){
        item.fund_id = fund_id;
        item.fund_name = fund_name;
        item.manager_name = manager_name;
        item.raising_address = raising_address;
        item.fund_admin = fund_admin;
        item.start_time = start_time;
        item.end_time = end_time;
        item.fund_size = fund_size;
    });
}

void FundContract::deletefund(uint64_t fund_id) {
    require_auth(_self);

    FundTable funds(_self, _self.value);
    auto itr = funds.find(fund_id);
    eosio_assert(itr != funds.end(), "fund does not exist");
    eosio_assert(itr->navs.empty(), "cannot delete fund with recorded nav");
    funds.erase(itr);
}

void FundContract::updatesize(uint64_t fund_id, eosio::asset subscription) {
    FundTable funds(_self, _self.value);
    auto itr = funds.find(fund_id);
    uint64_t now = publication_time();
    eosio_assert(itr != funds.end(), "fund does not exist");
    eosio_assert(subscription.amount >= 0, "subscription should not be negative");
    eosio_assert(subscription.is_valid(), "subscription is invalid");
    eosio_assert(subscription.symbol == itr->fund_size.symbol, "inconsistent symbol");
    require_auth(itr->fund_admin);
    eosio_assert(itr->navs.empty(), "cannot update size of fund with recorded nav");

    funds.modify(itr, _self, [subscription](Fund& item) {
        item.fund_size += subscription;
    });
}

void FundContract::addnav(uint64_t fund_id, uint64_t value, uint64_t valuation_time) {
    FundTable funds(_self, _self.value);
    auto itr = funds.find(fund_id);
    uint64_t now = publication_time();
    eosio_assert(itr != funds.end(), "fund does not exist");
    eosio_assert(itr->start_time < now, "nav can only be recorded after fund starts");
    eosio_assert(now <= itr->end_time + DAY * 7, "nav can only be recorded within 7 days after fund ends");
    eosio_assert(value >= 0, "nav should not be negative");
    eosio_assert(value <= 0xFFFFFFFF, "nav is too large");
    eosio_assert(valuation_time <= now, "valuation_time should be in the past");
    eosio_assert(valuation_time >= itr->start_time + HOUR * 6, "valuation_time should be at least 6 hours after fund starts");
    if (valuation_time != itr->end_time) {
        eosio_assert(valuation_time <= itr->end_time - HOUR * 6, "non-final nav should be calculated at least 6 hours before fund ends");
        eosio_assert(valuation_time > now - DAY, "non-final nav must be recorded within 1 day after calculated");
    }
    if (itr->navs.size() > 0) {
        eosio_assert(valuation_time >= itr->navs.back().valuation_time + HOUR * 6, "a gap of 6 hours is required");
    }
    require_auth(itr->fund_admin);

    funds.modify(itr, _self, [value, valuation_time, now](Fund& item) {
        item.navs.push_back(Nav{
            .value = value,
            .valuation_time = valuation_time,
            .record_time = now
        });
    });
}

void FundContract::deletenav(uint64_t fund_id) {
    FundTable funds(_self, _self.value);
    auto itr = funds.find(fund_id);
    uint64_t now = publication_time();
    eosio_assert(itr != funds.end(), "fund does not exist");
    require_auth(itr->fund_admin);
    eosio_assert(itr->navs.size() > 0, "no nav to delete");
    eosio_assert(now < itr->navs.back().record_time + DAY, "nav can only be deleted within 1 day after recorded");

    funds.modify(itr, _self, [](Fund& item) {
        item.navs.pop_back();
    });
}

EOSIO_DISPATCH(FundContract, (addfund)(deletefund)(updatesize)(addnav)(deletenav))
