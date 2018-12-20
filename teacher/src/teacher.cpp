#include "teacher.hpp"

uint64_t teacher_stu::id = 1;
uint64_t start = 0;

void teacher_stu::over(account_name teacher) {
  require_auth(teacher);
  print("teache:Class over!");

  mshares mshare_table(_self, _self);

  start = 1;//存储动作调用状态
  mshare_table.emplace( teacher, [&]( auto& mshare ) {
    mshare.id = id;
    mshare.start = start;
  });
}

void teacher_stu::eat(account_name student) {
  require_auth(student);
  mshares mshare_table(_self, _self);
  auto it = mshare_table.find( id );
  if (it->start == 1) {
    print("student:Class over, go to eat!");
    mshare_table.erase( it );
  }
  else
    print("teacher:Class time, you can't eat");
}
