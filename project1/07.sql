SELECT V1.nickname
  FROM (SELECT CP.nickname, CP.level, T.hometown
          FROM CatchedPokemon CP
             , Trainer T
         WHERE CP.owner_id = T.id
       ) V1
 WHERE NOT EXISTS
       (SELECT NULL
          FROM (SELECT CP.level, T.hometown
                  FROM CatchedPokemon CP
                     , Trainer T
                 WHERE CP.owner_id = T.id
               ) V2
         WHERE V1.hometown = V2.hometown
           AND V1.level < V2.level
       )
 ORDER BY V1.nickname;
