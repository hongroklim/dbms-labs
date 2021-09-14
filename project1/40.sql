SELECT T.name
  FROM Trainer T
     , CatchedPokemon CP
     , Pokemon P
 WHERE T.id = CP.owner_id
   AND CP.pid = P.id
   AND P.name = 'Pikachu'
