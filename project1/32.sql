SELECT T.name, P.name, count(*)
  FROM Trainer T
     , CatchedPokemon CP
     , Pokemon P
 WHERE T.id = CP.owner_id
   AND CP.pid = P.id
   AND EXISTS
       (SELECT NULL
          FROM (SELECT CP.owner_id
                  FROM CatchedPokemon CP
                     , Pokemon P
                 WHERE CP.pid = P.id
                 GROUP BY CP.owner_id
                HAVING MAX(P.type) = MIN(P.type)
               ) V1
         WHERE V1.owner_id = T.id
       )
 GROUP BY T.id, P.id
 ORDER BY T.name, P.name;
