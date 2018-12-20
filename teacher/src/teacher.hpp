#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

using namespace eosio;

class teacher_stu : public eosio::contract{
    using contract::contract;
  public:
    teacher_stu( account_name self ) :
			contract( self ) {}

    void over(account_name teacher);

    void eat(account_name student);

  private:
    static uint64_t id;

    // @abi table
    struct mshare {
      uint64_t id;
      uint64_t start = 0;

      uint64_t primary_key() const { return id; }
    };

    typedef multi_index<N(mshare), mshare> mshares;

};

EOSIO_ABI( teacher_stu, (over)(eat))

//查看表数据：
//cleos get table class class mshare
