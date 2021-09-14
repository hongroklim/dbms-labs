SELECT T.name
  FROM Gym G
     , Trainer T
 WHERE G.leader_id = T.id
 ORDER BY T.name;
