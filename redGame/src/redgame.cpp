//============================================================================
// Name        : redGame.cpp
// Author      : wuxiaopeng
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//
// sudo eosio-cpp -o redgame.wasm redgame.cpp -abigen
// cleos set contract redgame /home/wxp/contracts/redgame
// cleos push action redgame deposit '[ "alice", "100.0000 SYS" ]' -p alice
// 获取表数据：cleos get table redgame redgame account
// 查看余额:cleos get currency balance eosio.token alice SYS
// 下注 ： cleos push action redgame offerbet '[ "3.0000 SYS", "bob", "50ed53fcdaf27f88d51ea4e835b1055efe779bb87e6cfdff47d28c88ffb27129" ]' -p bob
// 公布秘钥 ： cleos push action redgame reveal '[ "50ed53fcdaf27f88d51ea4e835b1055efe779bb87e6cfdff47d28c88ffb27129", "15fe76d25e124b08feb835f12e00a879bd15666a33786e64b655891fba7d6c12" ]' -p bob
//============================================================================

#include "redgame.hpp"

void redgame::deposit(const name from, const asset& quantity) {	  //下注抵押资金,相当于玩家向开发者转账
	require_auth(from);
	account_index accounts(_code, _code.value);
	eosio_assert(quantity.amount > 1, "must deposit positive quantity");
	auto itr = accounts.find(from.value);
	//在账户列表中查找要转出代币的账号是否存在
	if (itr == accounts.end()) {
		//不存在的话就插入账号列表
		itr = accounts.emplace(_code, [&](auto& acnt)
		{
				acnt.owner = from;
				//这里需要初始化：3050003 error,attempt to add asset with different symbol
				acnt.eos_balance = quantity;
			});
	} else {
		accounts.modify(itr, _code, [&]( auto& acnt ) {
			acnt.eos_balance += quantity;
		});
	}
	//对eosio.token的transfer进行合约调用，玩家开始下注资金
	//账户(from)对账户( _self)进行转账，对应于游戏中，抵押代币到
	//向dice转账，由于使用的是eos token，转账通过eosio.token合约执行
	action(
			permission_level { from, "active"_n },
			"eosio.token"_n,
			"transfer"_n,
			std::make_tuple(from, _code, quantity, std::string(""))).send();
}



void redgame::withdraw(const name to, const asset& quantity) {
	account_index accounts(_code, _code.value);
	//胜方赢取资金
	require_auth(to);
	eosio_assert(quantity.is_valid(), "invalid quantity");
	eosio_assert(quantity.amount > 0, "must withdraw positive quantity");
	auto itr = accounts.find(to.value);
	//在账户列表中查找要转入代币的账号是否存在
	eosio_assert(itr != accounts.end(), "unknown account");

	accounts.modify(itr, to, [&](auto& acnt) {
		//转出代币
			eosio_assert( acnt.eos_balance >= quantity, "insufficient balance" );
			acnt.eos_balance -= quantity;
	});
	//对eosio.token的transfer进行合约调用，Dice主账号对胜利一方发送资金
	//代表在资金撤回操作中，
	//通过调用eosio.token的transfer，账户(_self)对账户(to)进行转账，
	//对应于游戏中，Dice主账户为胜利的一方发放所赢的赌注
	//提现动作
	//相当于：cleos push action eosio.token transfer '[_self, from, quantity]' -p _self@active
	action(
			permission_level {_self, "active"_n },
			"eosio.token"_n,
			"transfer"_n,
			std::make_tuple(_self, to, quantity, std::string(""))).send();
	//如果账户余额为0，则删除账户
	if (itr->is_empty()) {
		accounts.erase(itr);
	}
}

  void redgame::offerbet(const asset &bet, const name player, const capi_checksum256 &commitment)
  {
	  account_index accounts(_code, _code.value);
	  offer_index offers(_code, _code.value);
	  game_index games(_code, _code.value);
	  global_dice_index global_dices(_code, _code.value);
      //下注
	  //eosio_assert 参考 https://developers.eos.io/eosio-cpp/reference#eosio_assert
	  //验证下注货币
      eosio_assert(bet.symbol == symbol("SYS", 4), "only core token allowed");
      //判断是否有效
      eosio_assert(bet.is_valid(), "invalid bet");
      eosio_assert(bet.amount > 0, "must bet positive quantity");

      //如果下注已经存在，则退出
      eosio_assert(!has_offer(commitment), "offer with this commitment already exist");
      require_auth(player);

      auto cur_player_itr = accounts.find(player.value);                 //查找用户，accounts是账户数据表
      eosio_assert(cur_player_itr != accounts.end(), "unknown account"); //判断用户是否存在

      // Store new offer
      auto new_offer_itr = offers.emplace(_self, [&](auto &offer) {           //在下注的数据表中，用emplace添加数据
          offer.id         = offers.available_primary_key();                  //使用自增型主键
          offer.bet        = bet;
          offer.owner      = player;
          offer.commitment = commitment;
          offer.gameid     = 0;   											  //游戏id
      });
      // Try to find a matching bet
      // 寻找是否相同金额的下注方
      auto idx = offers.template get_index<"bet"_n>();
      //表示查询最低值边界
      auto matched_offer_itr = idx.lower_bound((uint64_t)new_offer_itr->bet.amount);

      if (matched_offer_itr == idx.end() || matched_offer_itr->bet != new_offer_itr->bet || matched_offer_itr->owner == new_offer_itr->owner)
      {
          // No matching bet found, update player's account
          accounts.modify( cur_player_itr, _self, [&](auto& acnt) {
             eosio_assert( acnt.eos_balance >= bet, "insufficient balance" );//充值额不够
             acnt.eos_balance -= bet; //扣除余额
             acnt.open_offers++;      //下注次数+1
          });
       } else {
         // Create global game counter if not exists
          auto gdice_itr = global_dices.begin();
          if( gdice_itr == global_dices.end() ) {
             gdice_itr = global_dices.emplace(_self, [&](auto& gdice){
                gdice.nextgameid=0;
             });
          }

          // Increment global game counter
          global_dices.modify(gdice_itr, _self, [&](auto& gdice){
             gdice.nextgameid++;
          });

          // Create a new game
            auto game_itr = games.emplace(_self, [&](auto& new_game){
            new_game.id       = gdice_itr->nextgameid;
            new_game.bet      = new_offer_itr->bet;
            new_game.deadline = eosio::time_point_sec(0);

            new_game.player1.commitment = matched_offer_itr->commitment;
		    memset(&new_game.player1.reveal, 0, sizeof(capi_checksum256));

		    new_game.player2.commitment = new_offer_itr->commitment;
		    memset(&new_game.player2.reveal, 0, sizeof(capi_checksum256));
          });

          // Update player's offers
            idx.modify(matched_offer_itr, _self, [&](auto& offer){
             offer.bet.amount = bet.amount;
             offer.gameid = game_itr->id;
          });

          offers.modify(new_offer_itr, _self, [&](auto& offer){
             offer.bet.amount = bet.amount;
             offer.gameid = game_itr->id;
          });

          // Update player's accounts
          accounts.modify( accounts.find( matched_offer_itr->owner.value ), _self, [&](auto& acnt) {
             acnt.open_offers--;
             acnt.open_games++;
          });

          accounts.modify( cur_player_itr, _self, [&](auto& acnt) {
             eosio_assert( acnt.eos_balance >= bet, "insufficient balance" );
             acnt.eos_balance -= bet;
             acnt.open_games++;
          });
       }
    }

   void redgame::canceloffer( const capi_checksum256& commitment ) {

	  account_index accounts(_code, _code.value);
	  offer_index offers(_code, _code.value);
	  game_index games(_code, _code.value);
	  global_dice_index global_dices(_code, _code.value);

	  auto idx = offers.template get_index<"commitment"_n>();
	  auto offer_itr = idx.find( offer::get_commitment(commitment) );

	  eosio_assert( offer_itr != idx.end(), "offer does not exists" );
	  eosio_assert( offer_itr->gameid == 0, "unable to cancel offer" );
	  require_auth( offer_itr->owner );

	  auto acnt_itr = accounts.find(offer_itr->owner.value);
	  accounts.modify(acnt_itr, _self, [&](auto& acnt){
		 acnt.open_offers--;
		 acnt.eos_balance += offer_itr->bet;
	  });
	  idx.erase(offer_itr);
   }

	void redgame::reveal( const capi_checksum256& commitment, const capi_checksum256& source ) {
		account_index accounts(_code, _code.value);
		offer_index offers(_code, _code.value);
		game_index games(_code, _code.value);
		global_dice_index global_dices(_code, _code.value);
	   /**
	    * source：解密秘钥
	    * sizeof:获取对象所占内存，这里取hash值长度。
	    * commitment：加密hash值。
	    */
	   assert_sha256( (char *)&source, sizeof(source), (const capi_checksum256 *)&commitment );

	   //获取当前执行reveal的人的SHA256
	   auto idx = offers.template get_index<"commitment"_n>();
	   auto curr_revealer_offer = idx.find(offer::get_commitment(commitment));
	   eosio_assert(curr_revealer_offer != idx.end(), "offer not found");
	   eosio_assert(curr_revealer_offer->gameid > 0, "unable to reveal"); //如果已经开过

	   //查询对应的game
	   auto game_itr = games.find( curr_revealer_offer->gameid);
	   eosio_assert(game_itr != games.end(), "games does not exist");

	   player curr_reveal = game_itr->player1; //获取玩家1
	   player prev_reveal = game_itr->player2; //获取玩家2

	   if( !is_equal(curr_reveal.commitment, commitment) ) {
		  //交换位置
		  std::swap(curr_reveal, prev_reveal);
	   }
	   eosio_assert( is_zero(curr_reveal.reveal) == true, "player already revealed");//判断玩家是否已经开奖

	   if( !is_zero(prev_reveal.reveal) ) {
		  //比较双方随机数的大小
		  capi_checksum256 result;
		  //检验hash值
		  sha256( (char *)&game_itr->player1, sizeof(player)*2, &result);

		  auto prev_revealer_offer = idx.find( offer::get_commitment(prev_reveal.commitment) );
		  eosio_assert(prev_revealer_offer != idx.end(), "prev_revealer_offer does not exist");

		  int winner = result.hash[1] < result.hash[0] ? 0 : 1;
		  print("winner", winner);
		  if( winner ) {
			 print("play1 win");
			 pay_and_clean(*game_itr, *curr_revealer_offer, *prev_revealer_offer);
		  } else {
			 print("play2 win");
			 pay_and_clean(*game_itr, *prev_revealer_offer, *curr_revealer_offer);
		  }
	   } else {
		  print("else");
		  games.modify(game_itr, _self, [&](auto& game){
			 if( is_equal(curr_reveal.commitment, game.player1.commitment) ){
				 game.player1.reveal = source;
			 }
			 else{
				 game.player2.reveal = source;
				 game.deadline = eosio::time_point_sec(now() + FIVE_MINUTES);
			 }
		  });
	   }
	}

	void redgame::claimexpired( const uint64_t gameid ) {
	   account_index accounts(_code, _code.value);
	   offer_index offers(_code, _code.value);
	   game_index games(_code, _code.value);
	   global_dice_index global_dices(_code, _code.value);
	   auto game_itr = games.find(gameid);
	   eosio_assert(game_itr != games.end(), "game not found");
	   eosio_assert(game_itr->deadline != eosio::time_point_sec(0) && eosio::time_point_sec(now()) > game_itr->deadline, "game not expired");

	   auto idx = offers.template get_index<"commitment"_n>();
	   auto player1_offer = idx.find( offer::get_commitment(game_itr->player1.commitment) );
	   auto player2_offer = idx.find( offer::get_commitment(game_itr->player2.commitment) );

	   if( !is_zero(game_itr->player1.reveal) ) {
		  eosio_assert( is_zero(game_itr->player2.reveal), "game error");
		  pay_and_clean(*game_itr, *player1_offer, *player2_offer);
	   } else {
		  eosio_assert( is_zero(game_itr->player1.reveal), "game error");
		  pay_and_clean(*game_itr, *player2_offer, *player1_offer);
	   }

	}

  void redgame::erasetest(name user, uint64_t gameid) {
	require_auth(user);
	game_index games(_code, _code.value);
	auto iterator = games.find(gameid);
	eosio_assert(iterator != games.end(), "games does not exist");
	games.erase(iterator);
  }

EOSIO_DISPATCH( redgame, (deposit)(withdraw)(offerbet)(claimexpired)(reveal)(erasetest)(canceloffer))
