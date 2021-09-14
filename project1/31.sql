SELECT P.name
  FROM Pokemon P
     , CatchedPokemon CP
     , Trainer T
 WHERE P.id = CP.pid
   AND CP.owner_id = T.id
   AND T.hometown IN ('Sangnok city', 'Blue city')
 ORDER BY P.name;
