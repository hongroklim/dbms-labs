SELECT V1.name, V1.total_level
  FROM (SELECT T.name, SUM(CP.level) AS total_level
          FROM Trainer T
             , CatchedPokemon CP
         WHERE T.id = CP.owner_id
         GROUP BY T.id
       ) V1
 WHERE NOT EXISTS
       (SELECT NULL
          FROM (SELECT SUM(CP.level) AS total_level
                  FROM Trainer T
                     , CatchedPokemon CP
                 WHERE T.id = CP.owner_id
                 GROUP BY T.id
               ) V2
         WHERE V2.total_level > V1.total_level
       );
