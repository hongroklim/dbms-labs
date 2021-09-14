SELECT T.name
  FROM Trainer T
     , Gym G
     , City C
 WHERE T.id = G.leader_id
   AND T.hometown = C.name
   AND C.description LIKE '%Amazon%';
