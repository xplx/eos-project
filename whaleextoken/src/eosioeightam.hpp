/**
 * This file is part of eosioeightam smart contract, which is published at
 * https://github.com/8amfund/eosioeightam. See LICENSE file in the project root
 * for full license information.
 */
#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

/**
 * Net asset value.
 */
struct Nav {
    /**
     * Fixed point value with four decimal places.
     *
     * For example, value 12345 in this field indicates NAV 1.2345, that is, 23.45% profit.
     */
    uint64_t value;

    /**
     * Timestamp in microseconds when this NAV is calculated.
     */
    uint64_t valuation_time;

    /**
     * Timestamp in microseconds when this NAV is recorded in this contract (block timestamp).
     */
    uint64_t record_time;
};

struct [[eosio::table, eosio::contract("FundContract")]] Fund {
    /**
     * Unique ID of this fund.
     */
    uint64_t fund_id;

    /**
     * Human-readable fund name.
     */
    std::string fund_name;

    /**
     * Human-readable fund manager name.
     */
    std::string manager_name;

    /**
     * Raising address of this fund.
     *
     * Fund assets are collected in this address before the fund starts,
     * and the fund manager returns fund assets back to this address after the fund ends.
     *
     * Do NOT transfer to this address after the fund's start_time unless you are the fund manager.
     */
    std::string raising_address;

    /**
     * EOS account who has permission to record NAVs for this fund.
     */
    eosio::name fund_admin;

    /**
     * Timestamp in microseconds when the fund starts.
     */
    uint64_t start_time;

    /**
     * Timestamp in microseconds when the fund ends.
     */
    uint64_t end_time;

    /**
     * Total amount of assets raised by this fund.
     *
     * Symbol of this field indicates currency type of this fund, which must be one of the following:
     * - BTC, 8 decimal places
     * - USDT, 8 decimal places
     * - ETH, 9 decimal places
     * - EOS, 4 decimal places
     */
    eosio::asset fund_size;

    /**
     * List of net asset value records.
     *
     * NAVs must be recorded in chronological order with at least 6 hour intervals.
     * NAV at the start time is 1 (raw value 10000) implicitly and not recorded.
     * The final NAV should be recorded with valuation_time equal to the end time exactly.
     */
    std::vector<Nav> navs;

    uint64_t primary_key() const { return fund_id; }
};

typedef eosio::multi_index<"fund"_n, Fund> FundTable;

/**
 * Contract to record fund information and NAVs on the blockchain.
 */
class [[eosio::contract("FundContract")]] FundContract : public eosio::contract {
    public:
        using contract::contract;

        //没有初始化结构体
        /**
         * Add a new fund.
         */
        [[eosio::action]]
        void addfund(
            uint64_t fund_id, std::string fund_name, std::string manager_name,
            std::string raising_address,
            eosio::name fund_admin,
            uint64_t start_time, uint64_t end_time, eosio::asset fund_size
        );

        /**
         * Delete a fund.
         *
         * Only funds with no NAV recorded can be deleted.
         */
        [[eosio::action]]
        void deletefund(uint64_t fund_id);

        /**
         * Update fund size with a new subscription.
         *
         * Only funds with no NAV recorded can be updated.
         *
         * @param fund_id - The fund ID.
         * @param subscription - A new subscription, which will be added to the fund size.
         */
        [[eosio::action]]
        void updatesize(uint64_t fund_id, eosio::asset subscription);

        /**
         * Record an NAV.
         *
         * @param fund_id - The fund ID.
         * @param value - Raw value, a fixed point number with four decimal places.
         * @param valuation_time - Timestamp in microseconds when this NAV is calculated.
         */
        [[eosio::action]]
        void addnav(uint64_t fund_id, uint64_t value, uint64_t valuation_time);

        /**
         * Delete the last recorded NAV.
         *
         * NAV can only be deleted within one day after it is recorded.
         *
         * @param fund_id - The fund ID.
         */
        [[eosio::action]]
        void deletenav(uint64_t fund_id);
};
