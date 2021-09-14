SELECT T.name
  FROM Trainer T
     , Gym G
 WHERE T.id = G.leader_id
   AND G.city = 'Brown city';
