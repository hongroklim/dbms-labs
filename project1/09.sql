SELECT V4.name, V3.nickname
  FROM (SELECT T.id, T.name
          FROM Trainer T
             , CatchedPokemon CP
         WHERE T.id = CP.owner_id
         GROUP BY T.id
        HAVING count(*) >= 4
       ) V4
    , (SELECT V1.owner_id, V1.nickname
         FROM (SELECT CP.owner_id, CP.level, CP.nickname
                 FROM CatchedPokemon CP
                    , Trainer T
                WHERE CP.owner_id = T.id
              ) V1
        WHERE NOT EXISTS
            (SELECT NULL
               FROM (SELECT CP.owner_id, CP.level
                       FROM CatchedPokemon CP
                          , Trainer T
                      WHERE CP.owner_id = T.id
                    ) V2
              WHERE V1.owner_id = V2.owner_id
                AND V1.level < V2.level
            )
       ) V3
 WHERE V4.id = V3.owner_id
 ORDER BY V3.nickname;
