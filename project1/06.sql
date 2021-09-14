SELECT T.name
     , AVG(CP.level)
  FROM Trainer T
     , Gym G
     , CatchedPokemon CP
 WHERE T.id = CP.owner_id
   AND T.id = G.leader_id
 GROUP BY T.name
 ORDER BY T.name;
