SELECT P.name
  FROM Pokemon P
 WHERE NOT EXISTS
       (SELECT NULL
          FROM CatchedPokemon CP
         WHERE CP.pid = P.id
       )
 ORDER BY P.name;
