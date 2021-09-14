SELECT CP.nickname
  FROM CatchedPokemon CP
     , Pokemon P
     , Trainer T
     , Gym G
 WHERE CP.pid = P.id
   AND CP.owner_id = T.id
   AND T.id = G.leader_id
   AND P.type = 'water'
   AND G.city = 'Sangnok City'
 ORDER BY CP.nickname;
