1、创建游戏：
js4eos push action wuxiaopeng12 create '["wuxiaopeng11","wuxiaopeng12"]' -p wuxiaopeng12
-- create {"challenger":"wuxiaopeng11","host":"wuxiaopeng12"}
查询表数据验证：
js4eos get table wuxiaopeng12 wuxiaopeng12 games
返回如下数据则表明成功：
{
    "rows": [
        {
            "challenger": "wuxiaopeng11",
            "host": "wuxiaopeng12",
            "turn": "wuxiaopeng12",
            "winner": "none",
            "board": [
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0
            ]
        }
    ],
    "more": false
}
