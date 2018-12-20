/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>  //eosio时间管理库
#include <eosiolib/crypto.h>
#include <eosiolib/print.hpp>

using namespace eosio;

class [[eosio::contract]]redgame: public eosio::contract {
public:
	using contract::contract;
	//构造函数初始化合约
	redgame(name receiver, name code, datastream<const char*> ds) :
			contract(receiver, code, ds) {
	}


	const uint32_t FIVE_MINUTES = 5 * 60;

	[[eosio::action]]
	void deposit(const name from, const asset& quantity);

	[[eosio::action]]
	void offerbet(const asset &bet, const name player, const capi_checksum256 &commitment);

	[[eosio::action]]
	void withdraw(const name to, const asset& quantity);

	[[eosio::action]]
	void claimexpired(const uint64_t gameid);

	[[eosio::action]]
	void reveal( const capi_checksum256& commitment, const capi_checksum256& source );

	[[eosio::action]]
	void erasetest(name user, uint64_t gameid);

	[[eosio::action]]
	void canceloffer( const capi_checksum256& commitment );
private:
	struct [[eosio::table]] account {
		name owner;
		asset eos_balance;
		uint32_t open_offers = 0;
		uint32_t open_games = 0;
		bool is_empty() const {
			return !(eos_balance.amount | open_offers | open_games);
		}
		uint64_t primary_key() const {
			return owner.value;
		}
	};
	typedef eosio::multi_index<"account"_n, account> account_index;

	struct [[eosio::table]] offer {
		uint64_t id;
		name owner;
		asset bet;
		capi_checksum256 commitment;

		uint64_t gameid = 0;

		uint64_t primary_key() const {
			return id;
		}

		uint64_t by_bet() const {
			return (uint64_t) bet.amount;
		}

		key256 by_commitment() const {
			return get_commitment(commitment);
		}

		static key256 get_commitment(const capi_checksum256& commitment) {
			const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&commitment);
			return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
		}
	};

	//多索引定义
	typedef eosio::multi_index<"offer"_n, offer, indexed_by<"bet"_n, const_mem_fun<offer, uint64_t, &offer::by_bet> >,
			indexed_by<"commitment"_n, const_mem_fun<offer, key256, &offer::by_commitment> >> offer_index;

	struct player //玩家数据结构，这个不用做成数据表，不需要 @abi table
	{
		capi_checksum256 commitment;       //玩家的SHA256
		capi_checksum256 reveal;           //玩家的随机密钥，这个密钥用来生成上面的SHA256
	};

	struct [[eosio::table]] game {
		uint64_t id;                       //游戏ID
		asset bet;                         //下注树
		eosio::time_point_sec deadline;    //到期时间,加5分钟
		player player1;                      //玩家1
		player player2;                      //玩家2
		uint64_t primary_key() const {
			return id;
		}  //设置主键
	};
	typedef eosio::multi_index<"game"_n, game> game_index; //定义一个game类型：game_index

	/**
	 * 计数器
	 */
	struct [[eosio::table]] global_dice {
		uint64_t id = 0;
		uint64_t nextgameid = 0;
		uint64_t primary_key() const {
			return id;
		}
	};

	typedef eosio::multi_index<"global"_n, global_dice> global_dice_index;

	//内部函数
	bool has_offer(const capi_checksum256 &commitment) const {
		offer_index offers(_code, _code.value);
		//是否已经下注
		auto idx = offers.template get_index<"commitment"_n>(); //在多索引数据表中查找非主键数据的方法
		auto itr = idx.find(offer::get_commitment(commitment));
		//不等于表示存在
		return itr != idx.end();
	}

	bool is_equal(const capi_checksum256 &a, const capi_checksum256 &b) const {
		return memcmp((void *) &a, (const void *) &b, sizeof(capi_checksum256)) == 0;
	}

	/**
	 * 判断是否为0
	 */
	bool is_zero(const capi_checksum256 &a) const {
		const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&a);
		return p64[0] == 0 && p64[1] == 0 && p64[2] == 0 && p64[3] == 0;
	}

	void pay_and_clean(const game &g, const offer &winner_offer, const offer &loser_offer) {
		offer_index offers(_code, _code.value);
		game_index games(_code, _code.value);
		account_index accounts(_code, _code.value);
		// Update winner account balance and game count
		auto winner_account = accounts.find(winner_offer.owner.value);
		accounts.modify(winner_account, _self, [&](auto &acnt) {
			acnt.eos_balance += 2 * g.bet;
			acnt.open_games--;
		});

		// Update losser account game count
		auto loser_account = accounts.find(loser_offer.owner.value);
		accounts.modify(loser_account, _self, [&](auto &acnt) {
			acnt.open_games--;
		});

		if (loser_account->is_empty()) {
			print("is_empty");
			accounts.erase(loser_account);
		}
		print("erase(g)");
		auto eraseGame = games.find(g.id);
		games.erase(eraseGame);

		print("winner_offer");
		auto idx = offers.template get_index<"commitment"_n>();
		auto winner = idx.find( offer::get_commitment(winner_offer.commitment));
		idx.erase(winner);

		auto eraseLoser = idx.find( offer::get_commitment(loser_offer.commitment));
		print("loser_offer");
		idx.erase(eraseLoser);
	}

};
