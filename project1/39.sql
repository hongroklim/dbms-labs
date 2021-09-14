SELECT P.name
  FROM Gym G
     , Trainer T
     , CatchedPokemon CP
     , Pokemon P
 WHERE G.leader_id = T.id
   AND T.id = CP.owner_id
   AND CP.pid = P.id
   AND G.city = 'Rainbow city'
 ORDER BY P.name;
