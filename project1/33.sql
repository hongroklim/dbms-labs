SELECT T.name
  FROM Trainer T
 WHERE EXISTS
       (SELECT NULL
          FROM CatchedPokemon CP
             , Pokemon P
         WHERE CP.pid = P.id
           AND CP.owner_id = T.id
           AND P.type = 'Psychic'
       )
 ORDER BY T.name;
