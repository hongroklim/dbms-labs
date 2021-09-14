SELECT T.name
  FROM Trainer T
     , (SELECT CP.owner_id, CP.pid, count(*) AS cnt
          FROM CatchedPokemon CP
         GROUP BY CP.owner_id, CP.pid
        HAVING count(*) >= 2
       ) V1
 WHERE T.id = V1.owner_id
 ORDER BY T.name;
