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
// ��ȡ�����ݣ�cleos get table redgame redgame account
// �鿴���:cleos get currency balance eosio.token alice SYS
// ��ע �� cleos push action redgame offerbet '[ "3.0000 SYS", "bob", "50ed53fcdaf27f88d51ea4e835b1055efe779bb87e6cfdff47d28c88ffb27129" ]' -p bob
// ������Կ �� cleos push action redgame reveal '[ "50ed53fcdaf27f88d51ea4e835b1055efe779bb87e6cfdff47d28c88ffb27129", "15fe76d25e124b08feb835f12e00a879bd15666a33786e64b655891fba7d6c12" ]' -p bob
//============================================================================

#include "redgame.hpp"

void redgame::deposit(const name from, const asset& quantity) {	  //��ע��Ѻ�ʽ�,�൱������򿪷���ת��
	require_auth(from);
	account_index accounts(_code, _code.value);
	eosio_assert(quantity.amount > 1, "must deposit positive quantity");
	auto itr = accounts.find(from.value);
	//���˻��б��в���Ҫת�����ҵ��˺��Ƿ����
	if (itr == accounts.end()) {
		//�����ڵĻ��Ͳ����˺��б�
		itr = accounts.emplace(_code, [&](auto& acnt)
		{
				acnt.owner = from;
				//������Ҫ��ʼ����3050003 error,attempt to add asset with different symbol
				acnt.eos_balance = quantity;
			});
	} else {
		accounts.modify(itr, _code, [&]( auto& acnt ) {
			acnt.eos_balance += quantity;
		});
	}
	//��eosio.token��transfer���к�Լ���ã���ҿ�ʼ��ע�ʽ�
	//�˻�(from)���˻�( _self)����ת�ˣ���Ӧ����Ϸ�У���Ѻ���ҵ�
	//��diceת�ˣ�����ʹ�õ���eos token��ת��ͨ��eosio.token��Լִ��
	action(
			permission_level { from, "active"_n },
			"eosio.token"_n,
			"transfer"_n,
			std::make_tuple(from, _code, quantity, std::string(""))).send();
}



void redgame::withdraw(const name to, const asset& quantity) {
	account_index accounts(_code, _code.value);
	//ʤ��Ӯȡ�ʽ�
	require_auth(to);
	eosio_assert(quantity.is_valid(), "invalid quantity");
	eosio_assert(quantity.amount > 0, "must withdraw positive quantity");
	auto itr = accounts.find(to.value);
	//���˻��б��в���Ҫת����ҵ��˺��Ƿ����
	eosio_assert(itr != accounts.end(), "unknown account");

	accounts.modify(itr, to, [&](auto& acnt) {
		//ת������
			eosio_assert( acnt.eos_balance >= quantity, "insufficient balance" );
			acnt.eos_balance -= quantity;
	});
	//��eosio.token��transfer���к�Լ���ã�Dice���˺Ŷ�ʤ��һ�������ʽ�
	//�������ʽ𳷻ز����У�
	//ͨ������eosio.token��transfer���˻�(_self)���˻�(to)����ת�ˣ�
	//��Ӧ����Ϸ�У�Dice���˻�Ϊʤ����һ��������Ӯ�Ķ�ע
	//���ֶ���
	//�൱�ڣ�cleos push action eosio.token transfer '[_self, from, quantity]' -p _self@active
	action(
			permission_level {_self, "active"_n },
			"eosio.token"_n,
			"transfer"_n,
			std::make_tuple(_self, to, quantity, std::string(""))).send();
	//����˻����Ϊ0����ɾ���˻�
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
      //��ע
	  //eosio_assert �ο� https://developers.eos.io/eosio-cpp/reference#eosio_assert
	  //��֤��ע����
      eosio_assert(bet.symbol == symbol("SYS", 4), "only core token allowed");
      //�ж��Ƿ���Ч
      eosio_assert(bet.is_valid(), "invalid bet");
      eosio_assert(bet.amount > 0, "must bet positive quantity");

      //�����ע�Ѿ����ڣ����˳�
      eosio_assert(!has_offer(commitment), "offer with this commitment already exist");
      require_auth(player);

      auto cur_player_itr = accounts.find(player.value);                 //�����û���accounts���˻����ݱ�
      eosio_assert(cur_player_itr != accounts.end(), "unknown account"); //�ж��û��Ƿ����

      // Store new offer
      auto new_offer_itr = offers.emplace(_self, [&](auto &offer) {           //����ע�����ݱ��У���emplace�������
          offer.id         = offers.available_primary_key();                  //ʹ������������
          offer.bet        = bet;
          offer.owner      = player;
          offer.commitment = commitment;
          offer.gameid     = 0;   											  //��Ϸid
      });
      // Try to find a matching bet
      // Ѱ���Ƿ���ͬ������ע��
      auto idx = offers.template get_index<"bet"_n>();
      //��ʾ��ѯ���ֵ�߽�
      auto matched_offer_itr = idx.lower_bound((uint64_t)new_offer_itr->bet.amount);

      if (matched_offer_itr == idx.end() || matched_offer_itr->bet != new_offer_itr->bet || matched_offer_itr->owner == new_offer_itr->owner)
      {
          // No matching bet found, update player's account
          accounts.modify( cur_player_itr, _self, [&](auto& acnt) {
             eosio_assert( acnt.eos_balance >= bet, "insufficient balance" );//��ֵ���
             acnt.eos_balance -= bet; //�۳����
             acnt.open_offers++;      //��ע����+1
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
	    * source��������Կ
	    * sizeof:��ȡ������ռ�ڴ棬����ȡhashֵ���ȡ�
	    * commitment������hashֵ��
	    */
	   assert_sha256( (char *)&source, sizeof(source), (const capi_checksum256 *)&commitment );

	   //��ȡ��ǰִ��reveal���˵�SHA256
	   auto idx = offers.template get_index<"commitment"_n>();
	   auto curr_revealer_offer = idx.find(offer::get_commitment(commitment));
	   eosio_assert(curr_revealer_offer != idx.end(), "offer not found");
	   eosio_assert(curr_revealer_offer->gameid > 0, "unable to reveal"); //����Ѿ�����

	   //��ѯ��Ӧ��game
	   auto game_itr = games.find( curr_revealer_offer->gameid);
	   eosio_assert(game_itr != games.end(), "games does not exist");

	   player curr_reveal = game_itr->player1; //��ȡ���1
	   player prev_reveal = game_itr->player2; //��ȡ���2

	   if( !is_equal(curr_reveal.commitment, commitment) ) {
		  //����λ��
		  std::swap(curr_reveal, prev_reveal);
	   }
	   eosio_assert( is_zero(curr_reveal.reveal) == true, "player already revealed");//�ж�����Ƿ��Ѿ�����

	   if( !is_zero(prev_reveal.reveal) ) {
		  //�Ƚ�˫��������Ĵ�С
		  capi_checksum256 result;
		  //����hashֵ
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
